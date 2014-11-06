// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache.h"

#include <string>

#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_util.h"
#include "content/browser/service_worker/service_worker_cache.pb.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/url_request/url_request_context.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerResponseType.h"

namespace content {

namespace {

typedef scoped_ptr<disk_cache::Backend> ScopedBackendPtr;
typedef base::Callback<void(bool)> BoolCallback;
typedef base::Callback<void(disk_cache::ScopedEntryPtr, bool)>
    EntryBoolCallback;
typedef base::Callback<void(scoped_ptr<ServiceWorkerCacheMetadata>)>
    MetadataCallback;

enum EntryIndex { INDEX_HEADERS = 0, INDEX_RESPONSE_BODY };

// The maximum size of an individual cache. Ultimately cache size is controlled
// per-origin.
const int kMaxCacheBytes = 512 * 1024 * 1024;

// Buffer size for cache and blob reading/writing.
const int kBufferSize = 1024 * 512;

void NotReachedCompletionCallback(int rv) {
  NOTREACHED();
}

blink::WebServiceWorkerResponseType ProtoResponseTypeToWebResponseType(
    ServiceWorkerCacheResponse::ResponseType response_type) {
  switch (response_type) {
    case ServiceWorkerCacheResponse::BASIC_TYPE:
      return blink::WebServiceWorkerResponseTypeBasic;
    case ServiceWorkerCacheResponse::CORS_TYPE:
      return blink::WebServiceWorkerResponseTypeCORS;
    case ServiceWorkerCacheResponse::DEFAULT_TYPE:
      return blink::WebServiceWorkerResponseTypeDefault;
    case ServiceWorkerCacheResponse::ERROR_TYPE:
      return blink::WebServiceWorkerResponseTypeError;
    case ServiceWorkerCacheResponse::OPAQUE_TYPE:
      return blink::WebServiceWorkerResponseTypeOpaque;
  }
  NOTREACHED();
  return blink::WebServiceWorkerResponseTypeOpaque;
}

ServiceWorkerCacheResponse::ResponseType WebResponseTypeToProtoResponseType(
    blink::WebServiceWorkerResponseType response_type) {
  switch (response_type) {
    case blink::WebServiceWorkerResponseTypeBasic:
      return ServiceWorkerCacheResponse::BASIC_TYPE;
    case blink::WebServiceWorkerResponseTypeCORS:
      return ServiceWorkerCacheResponse::CORS_TYPE;
    case blink::WebServiceWorkerResponseTypeDefault:
      return ServiceWorkerCacheResponse::DEFAULT_TYPE;
    case blink::WebServiceWorkerResponseTypeError:
      return ServiceWorkerCacheResponse::ERROR_TYPE;
    case blink::WebServiceWorkerResponseTypeOpaque:
      return ServiceWorkerCacheResponse::OPAQUE_TYPE;
  }
  NOTREACHED();
  return ServiceWorkerCacheResponse::OPAQUE_TYPE;
}

struct ResponseReadContext {
  ResponseReadContext(scoped_refptr<net::IOBufferWithSize> buff,
                      scoped_refptr<storage::BlobData> blob)
      : buffer(buff), blob_data(blob), total_bytes_read(0) {}

  scoped_refptr<net::IOBufferWithSize> buffer;
  scoped_refptr<storage::BlobData> blob_data;
  int total_bytes_read;

  DISALLOW_COPY_AND_ASSIGN(ResponseReadContext);
};

// Match callbacks
void MatchDidOpenEntry(scoped_ptr<ServiceWorkerFetchRequest> request,
                       const ServiceWorkerCache::ResponseCallback& callback,
                       base::WeakPtr<storage::BlobStorageContext> blob_storage,
                       scoped_ptr<disk_cache::Entry*> entryptr,
                       int rv);
void MatchDidReadMetadata(
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const ServiceWorkerCache::ResponseCallback& callback,
    base::WeakPtr<storage::BlobStorageContext> blob_storage,
    disk_cache::ScopedEntryPtr entry,
    scoped_ptr<ServiceWorkerCacheMetadata> headers);
void MatchDidReadResponseBodyData(
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const ServiceWorkerCache::ResponseCallback& callback,
    base::WeakPtr<storage::BlobStorageContext> blob_storage,
    disk_cache::ScopedEntryPtr entry,
    scoped_ptr<ServiceWorkerResponse> response,
    scoped_ptr<ResponseReadContext> response_context,
    int rv);
void MatchDoneWithBody(scoped_ptr<ServiceWorkerFetchRequest> request,
                       const ServiceWorkerCache::ResponseCallback& callback,
                       base::WeakPtr<storage::BlobStorageContext> blob_storage,
                       scoped_ptr<ServiceWorkerResponse> response,
                       scoped_ptr<ResponseReadContext> response_context);

// Delete callbacks
void DeleteDidOpenEntry(
    const GURL& origin,
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const ServiceWorkerCache::ErrorCallback& callback,
    scoped_ptr<disk_cache::Entry*> entryptr,
    const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
    int rv);

// Copy headers out of a cache entry and into a protobuf. The callback is
// guaranteed to be run.
void ReadMetadata(disk_cache::Entry* entry, const MetadataCallback& callback);
void ReadMetadataDidReadMetadata(
    disk_cache::Entry* entry,
    const MetadataCallback& callback,
    const scoped_refptr<net::IOBufferWithSize>& buffer,
    int rv);

// CreateBackend callbacks
void CreateBackendDidCreate(const ServiceWorkerCache::ErrorCallback& callback,
                            scoped_ptr<ScopedBackendPtr> backend_ptr,
                            base::WeakPtr<ServiceWorkerCache> cache,
                            int rv);

void MatchDidOpenEntry(scoped_ptr<ServiceWorkerFetchRequest> request,
                       const ServiceWorkerCache::ResponseCallback& callback,
                       base::WeakPtr<storage::BlobStorageContext> blob_storage,
                       scoped_ptr<disk_cache::Entry*> entryptr,
                       int rv) {
  if (rv != net::OK) {
    callback.Run(ServiceWorkerCache::ErrorTypeNotFound,
                 scoped_ptr<ServiceWorkerResponse>(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  DCHECK(entryptr);
  disk_cache::ScopedEntryPtr entry(*entryptr);

  // Copy the entry pointer before passing it in base::Bind.
  disk_cache::Entry* tmp_entry_ptr = entry.get();

  MetadataCallback headers_callback = base::Bind(MatchDidReadMetadata,
                                                 base::Passed(request.Pass()),
                                                 callback,
                                                 blob_storage,
                                                 base::Passed(entry.Pass()));

  ReadMetadata(tmp_entry_ptr, headers_callback);
}

bool VaryMatches(const ServiceWorkerHeaderMap& request,
                 const ServiceWorkerHeaderMap& cached_request,
                 const ServiceWorkerHeaderMap& response) {
  ServiceWorkerHeaderMap::const_iterator vary_iter = response.find("vary");
  if (vary_iter == response.end())
    return true;

  std::vector<std::string> vary_keys;
  Tokenize(vary_iter->second, ",", &vary_keys);
  for (std::vector<std::string>::const_iterator it = vary_keys.begin();
       it != vary_keys.end();
       ++it) {
    std::string trimmed;
    base::TrimWhitespaceASCII(*it, base::TRIM_ALL, &trimmed);
    if (trimmed == "*")
      return false;

    ServiceWorkerHeaderMap::const_iterator request_iter = request.find(trimmed);
    ServiceWorkerHeaderMap::const_iterator cached_request_iter =
        cached_request.find(trimmed);

    // If the header exists in one but not the other, no match.
    if ((request_iter == request.end()) !=
        (cached_request_iter == cached_request.end()))
      return false;

    // If the header exists in one, it exists in both. Verify that the values
    // are equal.
    if (request_iter != request.end() &&
        request_iter->second != cached_request_iter->second)
      return false;
  }

  return true;
}

void MatchDidReadMetadata(
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const ServiceWorkerCache::ResponseCallback& callback,
    base::WeakPtr<storage::BlobStorageContext> blob_storage,
    disk_cache::ScopedEntryPtr entry,
    scoped_ptr<ServiceWorkerCacheMetadata> metadata) {
  if (!metadata) {
    callback.Run(ServiceWorkerCache::ErrorTypeStorage,
                 scoped_ptr<ServiceWorkerResponse>(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  scoped_ptr<ServiceWorkerResponse> response(new ServiceWorkerResponse(
      request->url,
      metadata->response().status_code(),
      metadata->response().status_text(),
      ProtoResponseTypeToWebResponseType(metadata->response().response_type()),
      ServiceWorkerHeaderMap(),
      "",
      0));

  if (metadata->response().has_url())
    response->url = GURL(metadata->response().url());

  for (int i = 0; i < metadata->response().headers_size(); ++i) {
    const ServiceWorkerCacheHeaderMap header = metadata->response().headers(i);
    response->headers.insert(std::make_pair(header.name(), header.value()));
  }

  ServiceWorkerHeaderMap cached_request_headers;
  for (int i = 0; i < metadata->request().headers_size(); ++i) {
    const ServiceWorkerCacheHeaderMap header = metadata->request().headers(i);
    cached_request_headers[header.name()] = header.value();
  }

  if (!VaryMatches(
          request->headers, cached_request_headers, response->headers)) {
    callback.Run(ServiceWorkerCache::ErrorTypeNotFound,
                 scoped_ptr<ServiceWorkerResponse>(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  if (entry->GetDataSize(INDEX_RESPONSE_BODY) == 0) {
    callback.Run(ServiceWorkerCache::ErrorTypeOK,
                 response.Pass(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  // Stream the response body into a blob.
  if (!blob_storage) {
    callback.Run(ServiceWorkerCache::ErrorTypeStorage,
                 scoped_ptr<ServiceWorkerResponse>(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  response->blob_uuid = base::GenerateGUID();

  scoped_refptr<storage::BlobData> blob_data =
      new storage::BlobData(response->blob_uuid);
  scoped_refptr<net::IOBufferWithSize> response_body_buffer(
      new net::IOBufferWithSize(kBufferSize));

  scoped_ptr<ResponseReadContext> read_context(
      new ResponseReadContext(response_body_buffer, blob_data));

  // Copy the entry pointer before passing it in base::Bind.
  disk_cache::Entry* tmp_entry_ptr = entry.get();

  net::CompletionCallback read_callback =
      base::Bind(MatchDidReadResponseBodyData,
                 base::Passed(request.Pass()),
                 callback,
                 blob_storage,
                 base::Passed(entry.Pass()),
                 base::Passed(response.Pass()),
                 base::Passed(read_context.Pass()));

  int read_rv = tmp_entry_ptr->ReadData(INDEX_RESPONSE_BODY,
                                        0,
                                        response_body_buffer.get(),
                                        response_body_buffer->size(),
                                        read_callback);

  if (read_rv != net::ERR_IO_PENDING)
    read_callback.Run(read_rv);
}

void MatchDidReadResponseBodyData(
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const ServiceWorkerCache::ResponseCallback& callback,
    base::WeakPtr<storage::BlobStorageContext> blob_storage,
    disk_cache::ScopedEntryPtr entry,
    scoped_ptr<ServiceWorkerResponse> response,
    scoped_ptr<ResponseReadContext> response_context,
    int rv) {
  if (rv < 0) {
    callback.Run(ServiceWorkerCache::ErrorTypeStorage,
                 scoped_ptr<ServiceWorkerResponse>(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  if (rv == 0) {
    response->blob_uuid = response_context->blob_data->uuid();
    response->blob_size = response_context->total_bytes_read;
    MatchDoneWithBody(request.Pass(),
                      callback,
                      blob_storage,
                      response.Pass(),
                      response_context.Pass());
    return;
  }

  // TODO(jkarlin): This copying of the the entire cache response into memory is
  // awful. Create a new interface around SimpleCache that provides access the
  // data directly from the file. See bug http://crbug.com/403493.
  response_context->blob_data->AppendData(response_context->buffer->data(), rv);
  response_context->total_bytes_read += rv;
  int total_bytes_read = response_context->total_bytes_read;

  // Grab some pointers before passing them in bind.
  net::IOBufferWithSize* buffer = response_context->buffer.get();
  disk_cache::Entry* tmp_entry_ptr = entry.get();

  net::CompletionCallback read_callback =
      base::Bind(MatchDidReadResponseBodyData,
                 base::Passed(request.Pass()),
                 callback,
                 blob_storage,
                 base::Passed(entry.Pass()),
                 base::Passed(response.Pass()),
                 base::Passed(response_context.Pass()));

  int read_rv = tmp_entry_ptr->ReadData(INDEX_RESPONSE_BODY,
                                        total_bytes_read,
                                        buffer,
                                        buffer->size(),
                                        read_callback);

  if (read_rv != net::ERR_IO_PENDING)
    read_callback.Run(read_rv);
}

void MatchDoneWithBody(scoped_ptr<ServiceWorkerFetchRequest> request,
                       const ServiceWorkerCache::ResponseCallback& callback,
                       base::WeakPtr<storage::BlobStorageContext> blob_storage,
                       scoped_ptr<ServiceWorkerResponse> response,
                       scoped_ptr<ResponseReadContext> response_context) {
  if (!blob_storage) {
    callback.Run(ServiceWorkerCache::ErrorTypeStorage,
                 scoped_ptr<ServiceWorkerResponse>(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  scoped_ptr<storage::BlobDataHandle> blob_data_handle(
      blob_storage->AddFinishedBlob(response_context->blob_data.get()));

  callback.Run(ServiceWorkerCache::ErrorTypeOK,
               response.Pass(),
               blob_data_handle.Pass());
}

void DeleteDidOpenEntry(
    const GURL& origin,
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const ServiceWorkerCache::ErrorCallback& callback,
    scoped_ptr<disk_cache::Entry*> entryptr,
    const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
    int rv) {
  if (rv != net::OK) {
    callback.Run(ServiceWorkerCache::ErrorTypeNotFound);
    return;
  }

  DCHECK(entryptr);
  disk_cache::ScopedEntryPtr entry(*entryptr);

  if (quota_manager_proxy.get()) {
    quota_manager_proxy->NotifyStorageModified(
        storage::QuotaClient::kServiceWorkerCache,
        origin,
        storage::kStorageTypeTemporary,
        -1 * (entry->GetDataSize(INDEX_HEADERS) +
              entry->GetDataSize(INDEX_RESPONSE_BODY)));
  }

  entry->Doom();
  callback.Run(ServiceWorkerCache::ErrorTypeOK);
}

void ReadMetadata(disk_cache::Entry* entry, const MetadataCallback& callback) {
  DCHECK(entry);

  scoped_refptr<net::IOBufferWithSize> buffer(
      new net::IOBufferWithSize(entry->GetDataSize(INDEX_HEADERS)));

  net::CompletionCallback read_header_callback =
      base::Bind(ReadMetadataDidReadMetadata, entry, callback, buffer);

  int read_rv = entry->ReadData(
      INDEX_HEADERS, 0, buffer.get(), buffer->size(), read_header_callback);

  if (read_rv != net::ERR_IO_PENDING)
    read_header_callback.Run(read_rv);
}

void ReadMetadataDidReadMetadata(
    disk_cache::Entry* entry,
    const MetadataCallback& callback,
    const scoped_refptr<net::IOBufferWithSize>& buffer,
    int rv) {
  if (rv != buffer->size()) {
    callback.Run(scoped_ptr<ServiceWorkerCacheMetadata>());
    return;
  }

  scoped_ptr<ServiceWorkerCacheMetadata> metadata(
      new ServiceWorkerCacheMetadata());

  if (!metadata->ParseFromArray(buffer->data(), buffer->size())) {
    callback.Run(scoped_ptr<ServiceWorkerCacheMetadata>());
    return;
  }

  callback.Run(metadata.Pass());
}

void CreateBackendDidCreate(const ServiceWorkerCache::ErrorCallback& callback,
                            scoped_ptr<ScopedBackendPtr> backend_ptr,
                            base::WeakPtr<ServiceWorkerCache> cache,
                            int rv) {
  if (rv != net::OK || !cache) {
    callback.Run(ServiceWorkerCache::ErrorTypeStorage);
    return;
  }

  cache->set_backend(backend_ptr->Pass());
  callback.Run(ServiceWorkerCache::ErrorTypeOK);
}

}  // namespace

// Streams data from a blob and writes it to a given disk_cache::Entry.
class ServiceWorkerCache::BlobReader : public net::URLRequest::Delegate {
 public:
  typedef base::Callback<void(disk_cache::ScopedEntryPtr, bool)>
      EntryAndBoolCallback;

  BlobReader()
      : cache_entry_offset_(0),
        buffer_(new net::IOBufferWithSize(kBufferSize)),
        weak_ptr_factory_(this) {}

  // |entry| is passed to the callback once complete.
  void StreamBlobToCache(disk_cache::ScopedEntryPtr entry,
                         net::URLRequestContext* request_context,
                         scoped_ptr<storage::BlobDataHandle> blob_data_handle,
                         const EntryAndBoolCallback& callback) {
    DCHECK(entry);
    entry_ = entry.Pass();
    callback_ = callback;
    blob_request_ = storage::BlobProtocolHandler::CreateBlobRequest(
        blob_data_handle.Pass(), request_context, this);
    blob_request_->Start();
  }

  // net::URLRequest::Delegate overrides for reading blobs.
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override {
    NOTREACHED();
  }
  void OnAuthRequired(net::URLRequest* request,
                      net::AuthChallengeInfo* auth_info) override {
    NOTREACHED();
  }
  void OnCertificateRequested(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) override {
    NOTREACHED();
  }
  void OnSSLCertificateError(net::URLRequest* request,
                             const net::SSLInfo& ssl_info,
                             bool fatal) override {
    NOTREACHED();
  }
  void OnBeforeNetworkStart(net::URLRequest* request, bool* defer) override {
    NOTREACHED();
  }

  void OnResponseStarted(net::URLRequest* request) override {
    if (!request->status().is_success()) {
      callback_.Run(entry_.Pass(), false);
      return;
    }
    ReadFromBlob();
  }

  virtual void ReadFromBlob() {
    int bytes_read = 0;
    bool done =
        blob_request_->Read(buffer_.get(), buffer_->size(), &bytes_read);
    if (done)
      OnReadCompleted(blob_request_.get(), bytes_read);
  }

  void OnReadCompleted(net::URLRequest* request, int bytes_read) override {
    if (!request->status().is_success()) {
      callback_.Run(entry_.Pass(), false);
      return;
    }

    if (bytes_read == 0) {
      callback_.Run(entry_.Pass(), true);
      return;
    }

    net::CompletionCallback cache_write_callback =
        base::Bind(&BlobReader::DidWriteDataToEntry,
                   weak_ptr_factory_.GetWeakPtr(),
                   bytes_read);

    int rv = entry_->WriteData(INDEX_RESPONSE_BODY,
                               cache_entry_offset_,
                               buffer_.get(),
                               bytes_read,
                               cache_write_callback,
                               true /* truncate */);
    if (rv != net::ERR_IO_PENDING)
      cache_write_callback.Run(rv);
  }

  void DidWriteDataToEntry(int expected_bytes, int rv) {
    if (rv != expected_bytes) {
      callback_.Run(entry_.Pass(), false);
      return;
    }

    cache_entry_offset_ += rv;
    ReadFromBlob();
  }

 private:
  int cache_entry_offset_;
  disk_cache::ScopedEntryPtr entry_;
  scoped_ptr<net::URLRequest> blob_request_;
  EntryAndBoolCallback callback_;
  scoped_refptr<net::IOBufferWithSize> buffer_;
  base::WeakPtrFactory<BlobReader> weak_ptr_factory_;
};

// The state needed to pass between ServiceWorkerCache::Keys callbacks.
struct ServiceWorkerCache::KeysContext {
  KeysContext(const ServiceWorkerCache::RequestsCallback& callback,
              base::WeakPtr<ServiceWorkerCache> cache)
      : original_callback(callback),
        cache(cache),
        out_keys(new ServiceWorkerCache::Requests()),
        enumerated_entry(NULL) {}

  ~KeysContext() {
    for (size_t i = 0, max = entries.size(); i < max; ++i)
      entries[i]->Close();
    if (enumerated_entry)
      enumerated_entry->Close();
  }

  // The callback passed to the Keys() function.
  ServiceWorkerCache::RequestsCallback original_callback;

  // The ServiceWorkerCache that Keys was called on.
  base::WeakPtr<ServiceWorkerCache> cache;

  // The vector of open entries in the backend.
  Entries entries;

  // The output of the Keys function.
  scoped_ptr<ServiceWorkerCache::Requests> out_keys;

  // Used for enumerating cache entries.
  scoped_ptr<disk_cache::Backend::Iterator> backend_iterator;
  disk_cache::Entry* enumerated_entry;

  DISALLOW_COPY_AND_ASSIGN(KeysContext);
};

// The state needed to pass between ServiceWorkerCache::Put callbacks.
struct ServiceWorkerCache::PutContext {
  PutContext(
      const GURL& origin,
      scoped_ptr<ServiceWorkerFetchRequest> request,
      scoped_ptr<ServiceWorkerResponse> response,
      scoped_ptr<storage::BlobDataHandle> blob_data_handle,
      const ServiceWorkerCache::ResponseCallback& callback,
      base::WeakPtr<ServiceWorkerCache> cache,
      net::URLRequestContext* request_context,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy)
      : origin(origin),
        request(request.Pass()),
        response(response.Pass()),
        blob_data_handle(blob_data_handle.Pass()),
        callback(callback),
        cache(cache),
        request_context(request_context),
        quota_manager_proxy(quota_manager_proxy),
        cache_entry(NULL) {}
  ~PutContext() {
    if (cache_entry)
      cache_entry->Close();
  }

  // Input parameters to the Put function.
  GURL origin;
  scoped_ptr<ServiceWorkerFetchRequest> request;
  scoped_ptr<ServiceWorkerResponse> response;
  scoped_ptr<storage::BlobDataHandle> blob_data_handle;
  ServiceWorkerCache::ResponseCallback callback;
  base::WeakPtr<ServiceWorkerCache> cache;
  net::URLRequestContext* request_context;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy;

  // This isn't a scoped_ptr because the disk_cache needs an Entry** as input to
  // CreateEntry.
  disk_cache::Entry* cache_entry;

  // The BlobDataHandle for the output ServiceWorkerResponse.
  scoped_ptr<storage::BlobDataHandle> out_blob_data_handle;

  DISALLOW_COPY_AND_ASSIGN(PutContext);
};

// static
scoped_refptr<ServiceWorkerCache> ServiceWorkerCache::CreateMemoryCache(
    const GURL& origin,
    net::URLRequestContext* request_context,
    const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context) {
  return make_scoped_refptr(new ServiceWorkerCache(origin,
                                                   base::FilePath(),
                                                   request_context,
                                                   quota_manager_proxy,
                                                   blob_context));
}

// static
scoped_refptr<ServiceWorkerCache> ServiceWorkerCache::CreatePersistentCache(
    const GURL& origin,
    const base::FilePath& path,
    net::URLRequestContext* request_context,
    const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context) {
  return make_scoped_refptr(new ServiceWorkerCache(
      origin, path, request_context, quota_manager_proxy, blob_context));
}

ServiceWorkerCache::~ServiceWorkerCache() {
}

base::WeakPtr<ServiceWorkerCache> ServiceWorkerCache::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ServiceWorkerCache::Put(scoped_ptr<ServiceWorkerFetchRequest> request,
                             scoped_ptr<ServiceWorkerResponse> response,
                             const ResponseCallback& callback) {
  IncPendingOps();
  ResponseCallback pending_callback =
      base::Bind(&ServiceWorkerCache::PendingResponseCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback);
  scoped_ptr<storage::BlobDataHandle> blob_data_handle;

  if (!response->blob_uuid.empty()) {
    if (!blob_storage_context_) {
      pending_callback.Run(ErrorTypeStorage,
                           scoped_ptr<ServiceWorkerResponse>(),
                           scoped_ptr<storage::BlobDataHandle>());
      return;
    }
    blob_data_handle =
        blob_storage_context_->GetBlobDataFromUUID(response->blob_uuid);
    if (!blob_data_handle) {
      pending_callback.Run(ErrorTypeStorage,
                           scoped_ptr<ServiceWorkerResponse>(),
                           scoped_ptr<storage::BlobDataHandle>());
      return;
    }
  }

  scoped_ptr<PutContext> put_context(new PutContext(
      origin_, request.Pass(), response.Pass(), blob_data_handle.Pass(),
      pending_callback, weak_ptr_factory_.GetWeakPtr(), request_context_,
      quota_manager_proxy_));

  if (put_context->blob_data_handle) {
    // Grab another handle to the blob for the callback response.
    put_context->out_blob_data_handle =
        blob_storage_context_->GetBlobDataFromUUID(
            put_context->response->blob_uuid);
  }

  base::Closure continuation = base::Bind(&ServiceWorkerCache::PutImpl,
                                          base::Passed(put_context.Pass()));

  if (!initialized_) {
    Init(continuation);
    return;
  }

  continuation.Run();
}

void ServiceWorkerCache::Match(scoped_ptr<ServiceWorkerFetchRequest> request,
                               const ResponseCallback& callback) {
  IncPendingOps();
  ResponseCallback pending_callback =
      base::Bind(&ServiceWorkerCache::PendingResponseCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback);

  if (!initialized_) {
    Init(base::Bind(&ServiceWorkerCache::Match, weak_ptr_factory_.GetWeakPtr(),
                    base::Passed(request.Pass()), pending_callback));
    return;
  }
  if (!backend_) {
    pending_callback.Run(ErrorTypeStorage, scoped_ptr<ServiceWorkerResponse>(),
                         scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  scoped_ptr<disk_cache::Entry*> entry(new disk_cache::Entry*);

  disk_cache::Entry** entry_ptr = entry.get();

  ServiceWorkerFetchRequest* request_ptr = request.get();

  net::CompletionCallback open_entry_callback = base::Bind(
      MatchDidOpenEntry, base::Passed(request.Pass()), pending_callback,
      blob_storage_context_, base::Passed(entry.Pass()));

  int rv = backend_->OpenEntry(
      request_ptr->url.spec(), entry_ptr, open_entry_callback);
  if (rv != net::ERR_IO_PENDING)
    open_entry_callback.Run(rv);
}

void ServiceWorkerCache::Delete(scoped_ptr<ServiceWorkerFetchRequest> request,
                                const ErrorCallback& callback) {
  IncPendingOps();
  ErrorCallback pending_callback =
      base::Bind(&ServiceWorkerCache::PendingErrorCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback);

  if (!initialized_) {
    Init(base::Bind(&ServiceWorkerCache::Delete, weak_ptr_factory_.GetWeakPtr(),
                    base::Passed(request.Pass()), pending_callback));
    return;
  }
  if (!backend_) {
    pending_callback.Run(ErrorTypeStorage);
    return;
  }

  scoped_ptr<disk_cache::Entry*> entry(new disk_cache::Entry*);

  disk_cache::Entry** entry_ptr = entry.get();

  ServiceWorkerFetchRequest* request_ptr = request.get();

  net::CompletionCallback open_entry_callback = base::Bind(
      DeleteDidOpenEntry, origin_, base::Passed(request.Pass()),
      pending_callback, base::Passed(entry.Pass()), quota_manager_proxy_);

  int rv = backend_->OpenEntry(
      request_ptr->url.spec(), entry_ptr, open_entry_callback);
  if (rv != net::ERR_IO_PENDING)
    open_entry_callback.Run(rv);
}

void ServiceWorkerCache::Keys(const RequestsCallback& callback) {
  IncPendingOps();
  RequestsCallback pending_callback =
      base::Bind(&ServiceWorkerCache::PendingRequestsCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback);
  if (!initialized_) {
    Init(base::Bind(&ServiceWorkerCache::Keys, weak_ptr_factory_.GetWeakPtr(),
                    pending_callback));
    return;
  }
  if (!backend_) {
    pending_callback.Run(ErrorTypeStorage, scoped_ptr<Requests>());
    return;
  }

  // 1. Iterate through all of the entries, open them, and add them to a vector.
  // 2. For each open entry:
  //  2.1. Read the headers into a protobuf.
  //  2.2. Copy the protobuf into a ServiceWorkerFetchRequest (a "key").
  //  2.3. Push the response into a vector of requests to be returned.
  // 3. Return the vector of requests (keys).

  // The entries have to be loaded into a vector first because enumeration loops
  // forever if you read data from a cache entry while enumerating.

  scoped_ptr<KeysContext> keys_context(
      new KeysContext(pending_callback, weak_ptr_factory_.GetWeakPtr()));

  keys_context->backend_iterator = backend_->CreateIterator();
  disk_cache::Backend::Iterator& iterator = *keys_context->backend_iterator;
  disk_cache::Entry** enumerated_entry = &keys_context->enumerated_entry;

  net::CompletionCallback open_entry_callback =
      base::Bind(KeysDidOpenNextEntry, base::Passed(keys_context.Pass()));

  int rv = iterator.OpenNextEntry(enumerated_entry, open_entry_callback);

  if (rv != net::ERR_IO_PENDING)
    open_entry_callback.Run(rv);
}

void ServiceWorkerCache::Close(const base::Closure& callback) {
  DCHECK(!initialized_ || backend_)
      << "Don't call ServiceWorkerCache::Close() twice.";

  if (pending_ops_ > 0) {
    DCHECK(ops_complete_callback_.is_null());
    initialized_ = true;  // So that future operations halt.
    ops_complete_callback_ = base::Bind(
        &ServiceWorkerCache::Close, weak_ptr_factory_.GetWeakPtr(), callback);
    return;
  }

  initialized_ = true;
  backend_.reset();
  callback.Run();
}

int64 ServiceWorkerCache::MemoryBackedSize() const {
  if (!backend_ || !memory_only_)
    return 0;

  scoped_ptr<disk_cache::Backend::Iterator> backend_iter =
      backend_->CreateIterator();
  disk_cache::Entry* entry = nullptr;

  int64 sum = 0;

  std::vector<disk_cache::Entry*> entries;
  int rv = net::OK;
  while ((rv = backend_iter->OpenNextEntry(
              &entry, base::Bind(NotReachedCompletionCallback))) == net::OK) {
    entries.push_back(entry);  // Open the entries without mutating them.
  }
  DCHECK(rv !=
         net::ERR_IO_PENDING);  // Expect all memory ops to be synchronous.

  for (disk_cache::Entry* entry : entries) {
    sum += entry->GetDataSize(INDEX_HEADERS) +
           entry->GetDataSize(INDEX_RESPONSE_BODY);
    entry->Close();
  }

  return sum;
}

ServiceWorkerCache::ServiceWorkerCache(
    const GURL& origin,
    const base::FilePath& path,
    net::URLRequestContext* request_context,
    const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context)
    : origin_(origin),
      path_(path),
      request_context_(request_context),
      quota_manager_proxy_(quota_manager_proxy),
      blob_storage_context_(blob_context),
      initialized_(false),
      memory_only_(path.empty()),
      pending_ops_(0),
      weak_ptr_factory_(this) {
}

// static
void ServiceWorkerCache::PutImpl(scoped_ptr<PutContext> put_context) {
  if (!put_context->cache || !put_context->cache->backend_) {
    put_context->callback.Run(ErrorTypeStorage,
                              scoped_ptr<ServiceWorkerResponse>(),
                              scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  scoped_ptr<ServiceWorkerFetchRequest> request_copy(
      new ServiceWorkerFetchRequest(*put_context->request));
  ServiceWorkerCache* cache_ptr = put_context->cache.get();

  cache_ptr->Delete(request_copy.Pass(),
                    base::Bind(PutDidDelete, base::Passed(put_context.Pass())));
}

// static
void ServiceWorkerCache::PutDidDelete(scoped_ptr<PutContext> put_context,
                                      ErrorType delete_error) {
  if (!put_context->cache || !put_context->cache->backend_) {
    put_context->callback.Run(ErrorTypeStorage,
                              scoped_ptr<ServiceWorkerResponse>(),
                              scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  disk_cache::Entry** entry_ptr = &put_context->cache_entry;
  ServiceWorkerFetchRequest* request_ptr = put_context->request.get();
  disk_cache::Backend* backend_ptr = put_context->cache->backend_.get();

  net::CompletionCallback create_entry_callback =
      base::Bind(PutDidCreateEntry, base::Passed(put_context.Pass()));

  int create_rv = backend_ptr->CreateEntry(
      request_ptr->url.spec(), entry_ptr, create_entry_callback);

  if (create_rv != net::ERR_IO_PENDING)
    create_entry_callback.Run(create_rv);
}

// static
void ServiceWorkerCache::PutDidCreateEntry(scoped_ptr<PutContext> put_context,
                                           int rv) {
  if (rv != net::OK) {
    put_context->callback.Run(ServiceWorkerCache::ErrorTypeExists,
                              scoped_ptr<ServiceWorkerResponse>(),
                              scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  DCHECK(put_context->cache_entry);

  ServiceWorkerCacheMetadata metadata;
  ServiceWorkerCacheRequest* request_metadata = metadata.mutable_request();
  request_metadata->set_method(put_context->request->method);
  for (ServiceWorkerHeaderMap::const_iterator it =
           put_context->request->headers.begin();
       it != put_context->request->headers.end();
       ++it) {
    ServiceWorkerCacheHeaderMap* header_map = request_metadata->add_headers();
    header_map->set_name(it->first);
    header_map->set_value(it->second);
  }

  ServiceWorkerCacheResponse* response_metadata = metadata.mutable_response();
  response_metadata->set_status_code(put_context->response->status_code);
  response_metadata->set_status_text(put_context->response->status_text);
  response_metadata->set_response_type(
      WebResponseTypeToProtoResponseType(put_context->response->response_type));
  response_metadata->set_url(put_context->response->url.spec());
  for (ServiceWorkerHeaderMap::const_iterator it =
           put_context->response->headers.begin();
       it != put_context->response->headers.end();
       ++it) {
    ServiceWorkerCacheHeaderMap* header_map = response_metadata->add_headers();
    header_map->set_name(it->first);
    header_map->set_value(it->second);
  }

  scoped_ptr<std::string> serialized(new std::string());
  if (!metadata.SerializeToString(serialized.get())) {
    put_context->callback.Run(ServiceWorkerCache::ErrorTypeStorage,
                              scoped_ptr<ServiceWorkerResponse>(),
                              scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  scoped_refptr<net::StringIOBuffer> buffer(
      new net::StringIOBuffer(serialized.Pass()));

  // Get a temporary copy of the entry pointer before passing it in base::Bind.
  disk_cache::Entry* tmp_entry_ptr = put_context->cache_entry;

  net::CompletionCallback write_headers_callback = base::Bind(
      PutDidWriteHeaders, base::Passed(put_context.Pass()), buffer->size());

  rv = tmp_entry_ptr->WriteData(INDEX_HEADERS,
                                0 /* offset */,
                                buffer.get(),
                                buffer->size(),
                                write_headers_callback,
                                true /* truncate */);

  if (rv != net::ERR_IO_PENDING)
    write_headers_callback.Run(rv);
}

// static
void ServiceWorkerCache::PutDidWriteHeaders(scoped_ptr<PutContext> put_context,
                                            int expected_bytes,
                                            int rv) {
  if (rv != expected_bytes) {
    put_context->cache_entry->Doom();
    put_context->callback.Run(ServiceWorkerCache::ErrorTypeStorage,
                              scoped_ptr<ServiceWorkerResponse>(),
                              scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  // The metadata is written, now for the response content. The data is streamed
  // from the blob into the cache entry.

  if (put_context->response->blob_uuid.empty()) {
    if (put_context->quota_manager_proxy.get()) {
      put_context->quota_manager_proxy->NotifyStorageModified(
          storage::QuotaClient::kServiceWorkerCache,
          put_context->origin,
          storage::kStorageTypeTemporary,
          put_context->cache_entry->GetDataSize(INDEX_HEADERS));
    }

    put_context->callback.Run(ServiceWorkerCache::ErrorTypeOK,
                              put_context->response.Pass(),
                              scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  DCHECK(put_context->blob_data_handle);

  disk_cache::ScopedEntryPtr entry(put_context->cache_entry);
  put_context->cache_entry = NULL;
  scoped_ptr<BlobReader> reader(new BlobReader());
  BlobReader* reader_ptr = reader.get();

  // Grab some pointers before passing put_context in Bind.
  net::URLRequestContext* request_context = put_context->request_context;
  scoped_ptr<storage::BlobDataHandle> blob_data_handle =
      put_context->blob_data_handle.Pass();

  reader_ptr->StreamBlobToCache(entry.Pass(),
                                request_context,
                                blob_data_handle.Pass(),
                                base::Bind(PutDidWriteBlobToCache,
                                           base::Passed(put_context.Pass()),
                                           base::Passed(reader.Pass())));
}

// static
void ServiceWorkerCache::PutDidWriteBlobToCache(
    scoped_ptr<PutContext> put_context,
    scoped_ptr<BlobReader> blob_reader,
    disk_cache::ScopedEntryPtr entry,
    bool success) {
  DCHECK(entry);
  put_context->cache_entry = entry.release();

  if (!success) {
    put_context->cache_entry->Doom();
    put_context->callback.Run(ServiceWorkerCache::ErrorTypeStorage,
                              scoped_ptr<ServiceWorkerResponse>(),
                              scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  if (put_context->quota_manager_proxy.get()) {
    put_context->quota_manager_proxy->NotifyStorageModified(
        storage::QuotaClient::kServiceWorkerCache,
        put_context->origin,
        storage::kStorageTypeTemporary,
        put_context->cache_entry->GetDataSize(INDEX_HEADERS) +
            put_context->cache_entry->GetDataSize(INDEX_RESPONSE_BODY));
  }

  put_context->callback.Run(ServiceWorkerCache::ErrorTypeOK,
                            put_context->response.Pass(),
                            put_context->out_blob_data_handle.Pass());
}

// static
void ServiceWorkerCache::KeysDidOpenNextEntry(
    scoped_ptr<KeysContext> keys_context,
    int rv) {
  if (rv == net::ERR_FAILED) {
    DCHECK(!keys_context->enumerated_entry);
    // Enumeration is complete, extract the requests from the entries.
    Entries::iterator iter = keys_context->entries.begin();
    KeysProcessNextEntry(keys_context.Pass(), iter);
    return;
  }

  base::WeakPtr<ServiceWorkerCache> cache = keys_context->cache;
  if (rv < 0 || !cache) {
    keys_context->original_callback.Run(ErrorTypeStorage,
                                        scoped_ptr<Requests>());
    return;
  }

  if (!cache->backend_) {
    keys_context->original_callback.Run(ErrorTypeNotFound,
                                        scoped_ptr<Requests>());
    return;
  }

  // Store the entry.
  keys_context->entries.push_back(keys_context->enumerated_entry);
  keys_context->enumerated_entry = NULL;

  // Enumerate the next entry.
  disk_cache::Backend::Iterator& iterator = *keys_context->backend_iterator;
  disk_cache::Entry** enumerated_entry = &keys_context->enumerated_entry;
  net::CompletionCallback open_entry_callback =
      base::Bind(KeysDidOpenNextEntry, base::Passed(keys_context.Pass()));

  rv = iterator.OpenNextEntry(enumerated_entry, open_entry_callback);

  if (rv != net::ERR_IO_PENDING)
    open_entry_callback.Run(rv);
}

// static
void ServiceWorkerCache::KeysProcessNextEntry(
    scoped_ptr<KeysContext> keys_context,
    const Entries::iterator& iter) {
  if (iter == keys_context->entries.end()) {
    // All done. Return all of the keys.
    keys_context->original_callback.Run(ErrorTypeOK,
                                        keys_context->out_keys.Pass());
    return;
  }

  ReadMetadata(
      *iter,
      base::Bind(KeysDidReadMetadata, base::Passed(keys_context.Pass()), iter));
}

// static
void ServiceWorkerCache::KeysDidReadMetadata(
    scoped_ptr<KeysContext> keys_context,
    const Entries::iterator& iter,
    scoped_ptr<ServiceWorkerCacheMetadata> metadata) {
  disk_cache::Entry* entry = *iter;

  if (metadata) {
    keys_context->out_keys->push_back(
        ServiceWorkerFetchRequest(GURL(entry->GetKey()),
                                  metadata->request().method(),
                                  ServiceWorkerHeaderMap(),
                                  GURL(),
                                  false));

    ServiceWorkerHeaderMap& req_headers =
        keys_context->out_keys->back().headers;

    for (int i = 0; i < metadata->request().headers_size(); ++i) {
      const ServiceWorkerCacheHeaderMap header = metadata->request().headers(i);
      req_headers.insert(std::make_pair(header.name(), header.value()));
    }
  } else {
    entry->Doom();
  }

  KeysProcessNextEntry(keys_context.Pass(), iter + 1);
}

void ServiceWorkerCache::CreateBackend(const ErrorCallback& callback) {
  DCHECK(!backend_);

  // Use APP_CACHE as opposed to DISK_CACHE to prevent cache eviction.
  net::CacheType cache_type = memory_only_ ? net::MEMORY_CACHE : net::APP_CACHE;

  scoped_ptr<ScopedBackendPtr> backend_ptr(new ScopedBackendPtr());

  // Temporary pointer so that backend_ptr can be Pass()'d in Bind below.
  ScopedBackendPtr* backend = backend_ptr.get();

  net::CompletionCallback create_cache_callback =
      base::Bind(CreateBackendDidCreate,
                 callback,
                 base::Passed(backend_ptr.Pass()),
                 weak_ptr_factory_.GetWeakPtr());

  // TODO(jkarlin): Use the cache MessageLoopProxy that ServiceWorkerCacheCore
  // has for disk caches.
  int rv = disk_cache::CreateCacheBackend(
      cache_type,
      net::CACHE_BACKEND_SIMPLE,
      path_,
      kMaxCacheBytes,
      false, /* force */
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE).get(),
      NULL,
      backend,
      create_cache_callback);
  if (rv != net::ERR_IO_PENDING)
    create_cache_callback.Run(rv);
}

void ServiceWorkerCache::Init(const base::Closure& callback) {
  DCHECK(!initialized_);
  init_callbacks_.push_back(callback);

  // If this isn't the first call to Init then return as the initialization
  // has already started.
  if (init_callbacks_.size() > 1u)
    return;

  CreateBackend(base::Bind(&ServiceWorkerCache::InitDone,
                           weak_ptr_factory_.GetWeakPtr()));
}

void ServiceWorkerCache::InitDone(ErrorType error) {
  initialized_ = true;
  for (std::vector<base::Closure>::iterator it = init_callbacks_.begin();
       it != init_callbacks_.end();
       ++it) {
    it->Run();
  }
  init_callbacks_.clear();
}

void ServiceWorkerCache::DecPendingOps() {
  DCHECK(pending_ops_ > 0);
  pending_ops_--;
  if (pending_ops_ == 0 && !ops_complete_callback_.is_null()) {
    ops_complete_callback_.Run();
    ops_complete_callback_.Reset();
  }
}

void ServiceWorkerCache::PendingErrorCallback(const ErrorCallback& callback,
                                              ErrorType error) {
  callback.Run(error);
  DecPendingOps();
}

void ServiceWorkerCache::PendingResponseCallback(
    const ResponseCallback& callback,
    ErrorType error,
    scoped_ptr<ServiceWorkerResponse> response,
    scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
  callback.Run(error, response.Pass(), blob_data_handle.Pass());
  DecPendingOps();
}

void ServiceWorkerCache::PendingRequestsCallback(
    const RequestsCallback& callback,
    ErrorType error,
    scoped_ptr<Requests> requests) {
  callback.Run(error, requests.Pass());
  DecPendingOps();
}

}  // namespace content
