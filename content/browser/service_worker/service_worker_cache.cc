// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache.h"

#include <string>

#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/browser/service_worker/service_worker_cache.pb.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/url_request/url_request_context.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/blob/blob_storage_context.h"
#include "webkit/browser/blob/blob_url_request_job_factory.h"

namespace content {

namespace {

typedef scoped_ptr<disk_cache::Backend> ScopedBackendPtr;
typedef base::Callback<void(bool)> BoolCallback;
typedef base::Callback<void(disk_cache::ScopedEntryPtr, bool)>
    EntryBoolCallback;
typedef base::Callback<void(scoped_ptr<ServiceWorkerRequestResponseHeaders>)>
    HeadersCallback;

enum EntryIndex { INDEX_HEADERS = 0, INDEX_RESPONSE_BODY };

// The maximum size of an individual cache. Ultimately cache size is controlled
// per-origin.
const int kMaxCacheBytes = 512 * 1024 * 1024;

// Buffer size for cache and blob reading/writing.
const int kBufferSize = 1024 * 512;

struct ResponseReadContext {
  ResponseReadContext(scoped_refptr<net::IOBufferWithSize> buff,
                      scoped_refptr<storage::BlobData> blob)
      : buffer(buff), blob_data(blob), total_bytes_read(0) {}

  scoped_refptr<net::IOBufferWithSize> buffer;
  scoped_refptr<storage::BlobData> blob_data;
  int total_bytes_read;

  DISALLOW_COPY_AND_ASSIGN(ResponseReadContext);
};

// Streams data from a blob and writes it to a given disk_cache::Entry.
class BlobReader : public net::URLRequest::Delegate {
 public:
  typedef base::Callback<void(disk_cache::ScopedEntryPtr, bool)>
      EntryBoolCallback;

  BlobReader(disk_cache::ScopedEntryPtr entry)
      : cache_entry_offset_(0),
        buffer_(new net::IOBufferWithSize(kBufferSize)),
        weak_ptr_factory_(this) {
    DCHECK(entry);
    entry_ = entry.Pass();
  }

  void StreamBlobToCache(net::URLRequestContext* request_context,
                         scoped_ptr<storage::BlobDataHandle> blob_data_handle,
                         const EntryBoolCallback& callback) {
    callback_ = callback;
    blob_request_ = storage::BlobProtocolHandler::CreateBlobRequest(
        blob_data_handle.Pass(), request_context, this);
    blob_request_->Start();
  }

  // net::URLRequest::Delegate overrides for reading blobs.
  virtual void OnReceivedRedirect(net::URLRequest* request,
                                  const net::RedirectInfo& redirect_info,
                                  bool* defer_redirect) OVERRIDE {
    NOTREACHED();
  }
  virtual void OnAuthRequired(net::URLRequest* request,
                              net::AuthChallengeInfo* auth_info) OVERRIDE {
    NOTREACHED();
  }
  virtual void OnCertificateRequested(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) OVERRIDE {
    NOTREACHED();
  }
  virtual void OnSSLCertificateError(net::URLRequest* request,
                                     const net::SSLInfo& ssl_info,
                                     bool fatal) OVERRIDE {
    NOTREACHED();
  }
  virtual void OnBeforeNetworkStart(net::URLRequest* request,
                                    bool* defer) OVERRIDE {
    NOTREACHED();
  }

  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE {
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

  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE {
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
  EntryBoolCallback callback_;
  scoped_refptr<net::IOBufferWithSize> buffer_;
  base::WeakPtrFactory<BlobReader> weak_ptr_factory_;
};

// Put callbacks
void PutDidCreateEntry(ServiceWorkerFetchRequest* request,
                       ServiceWorkerResponse* response,
                       const ServiceWorkerCache::ErrorCallback& callback,
                       scoped_ptr<disk_cache::Entry*> entryptr,
                       scoped_ptr<storage::BlobDataHandle> blob_data_handle,
                       net::URLRequestContext* request_context,
                       int rv);
void PutDidWriteHeaders(ServiceWorkerResponse* response,
                        const ServiceWorkerCache::ErrorCallback& callback,
                        disk_cache::ScopedEntryPtr entry,
                        scoped_ptr<storage::BlobDataHandle> blob_data_handle,
                        net::URLRequestContext* request_context,
                        int expected_bytes,
                        int rv);
void PutDidWriteBlobToCache(const ServiceWorkerCache::ErrorCallback& callback,
                            scoped_ptr<BlobReader> blob_reader,
                            disk_cache::ScopedEntryPtr entry,
                            bool success);

// Match callbacks
void MatchDidOpenEntry(ServiceWorkerFetchRequest* request,
                       const ServiceWorkerCache::ResponseCallback& callback,
                       base::WeakPtr<storage::BlobStorageContext> blob_storage,
                       scoped_ptr<disk_cache::Entry*> entryptr,
                       int rv);
void MatchDidReadHeaderData(
    ServiceWorkerFetchRequest* request,
    const ServiceWorkerCache::ResponseCallback& callback,
    base::WeakPtr<storage::BlobStorageContext> blob_storage,
    disk_cache::ScopedEntryPtr entry,
    scoped_ptr<ServiceWorkerRequestResponseHeaders> headers);
void MatchDidReadResponseBodyData(
    ServiceWorkerFetchRequest* request,
    const ServiceWorkerCache::ResponseCallback& callback,
    base::WeakPtr<storage::BlobStorageContext> blob_storage,
    disk_cache::ScopedEntryPtr entry,
    scoped_ptr<ServiceWorkerResponse> response,
    scoped_ptr<ResponseReadContext> response_context,
    int rv);
void MatchDoneWithBody(ServiceWorkerFetchRequest* request,
                       const ServiceWorkerCache::ResponseCallback& callback,
                       base::WeakPtr<storage::BlobStorageContext> blob_storage,
                       scoped_ptr<ServiceWorkerResponse> response,
                       scoped_ptr<ResponseReadContext> response_context);

// Delete callbacks
void DeleteDidOpenEntry(ServiceWorkerFetchRequest* request,
                        const ServiceWorkerCache::ErrorCallback& callback,
                        scoped_ptr<disk_cache::Entry*> entryptr,
                        int rv);

// Copy headers out of a cache entry and into a protobuf. The callback is
// guaranteed to be run.
void ReadHeaders(disk_cache::Entry* entry, const HeadersCallback& callback);
void ReadHeadersDidReadHeaderData(
    disk_cache::Entry* entry,
    const HeadersCallback& callback,
    const scoped_refptr<net::IOBufferWithSize>& buffer,
    int rv);

// CreateBackend callbacks
void CreateBackendDidCreate(const ServiceWorkerCache::ErrorCallback& callback,
                            scoped_ptr<ScopedBackendPtr> backend_ptr,
                            base::WeakPtr<ServiceWorkerCache> cache,
                            int rv);

void PutDidCreateEntry(ServiceWorkerFetchRequest* request,
                       ServiceWorkerResponse* response,
                       const ServiceWorkerCache::ErrorCallback& callback,
                       scoped_ptr<disk_cache::Entry*> entryptr,
                       scoped_ptr<storage::BlobDataHandle> blob_data_handle,
                       net::URLRequestContext* request_context,
                       int rv) {
  if (rv != net::OK) {
    callback.Run(ServiceWorkerCache::ErrorTypeExists);
    return;
  }

  DCHECK(entryptr);
  disk_cache::ScopedEntryPtr entry(*entryptr);

  ServiceWorkerRequestResponseHeaders headers;
  headers.set_method(request->method);

  headers.set_status_code(response->status_code);
  headers.set_status_text(response->status_text);
  for (std::map<std::string, std::string>::const_iterator it =
           request->headers.begin();
       it != request->headers.end();
       ++it) {
    ServiceWorkerRequestResponseHeaders::HeaderMap* header_map =
        headers.add_request_headers();
    header_map->set_name(it->first);
    header_map->set_value(it->second);
  }

  for (std::map<std::string, std::string>::const_iterator it =
           response->headers.begin();
       it != response->headers.end();
       ++it) {
    ServiceWorkerRequestResponseHeaders::HeaderMap* header_map =
        headers.add_response_headers();
    header_map->set_name(it->first);
    header_map->set_value(it->second);
  }

  scoped_ptr<std::string> serialized(new std::string());
  if (!headers.SerializeToString(serialized.get())) {
    callback.Run(ServiceWorkerCache::ErrorTypeStorage);
    return;
  }

  scoped_refptr<net::StringIOBuffer> buffer(
      new net::StringIOBuffer(serialized.Pass()));

  // Get a temporary copy of the entry pointer before passing it in base::Bind.
  disk_cache::Entry* tmp_entry_ptr = entry.get();

  net::CompletionCallback write_headers_callback =
      base::Bind(PutDidWriteHeaders,
                 response,
                 callback,
                 base::Passed(entry.Pass()),
                 base::Passed(blob_data_handle.Pass()),
                 request_context,
                 buffer->size());

  rv = tmp_entry_ptr->WriteData(INDEX_HEADERS,
                                0 /* offset */,
                                buffer.get(),
                                buffer->size(),
                                write_headers_callback,
                                true /* truncate */);

  if (rv != net::ERR_IO_PENDING)
    write_headers_callback.Run(rv);
}

void PutDidWriteHeaders(ServiceWorkerResponse* response,
                        const ServiceWorkerCache::ErrorCallback& callback,
                        disk_cache::ScopedEntryPtr entry,
                        scoped_ptr<storage::BlobDataHandle> blob_data_handle,
                        net::URLRequestContext* request_context,
                        int expected_bytes,
                        int rv) {
  if (rv != expected_bytes) {
    entry->Doom();
    callback.Run(ServiceWorkerCache::ErrorTypeStorage);
    return;
  }

  // The metadata is written, now for the response content. The data is streamed
  // from the blob into the cache entry.

  if (response->blob_uuid.empty()) {
    callback.Run(ServiceWorkerCache::ErrorTypeOK);
    return;
  }

  DCHECK(blob_data_handle);

  scoped_ptr<BlobReader> reader(new BlobReader(entry.Pass()));
  BlobReader* reader_ptr = reader.get();

  reader_ptr->StreamBlobToCache(
      request_context,
      blob_data_handle.Pass(),
      base::Bind(
          PutDidWriteBlobToCache, callback, base::Passed(reader.Pass())));
}

void PutDidWriteBlobToCache(const ServiceWorkerCache::ErrorCallback& callback,
                            scoped_ptr<BlobReader> blob_reader,
                            disk_cache::ScopedEntryPtr entry,
                            bool success) {
  if (!success) {
    entry->Doom();
    callback.Run(ServiceWorkerCache::ErrorTypeStorage);
    return;
  }

  callback.Run(ServiceWorkerCache::ErrorTypeOK);
}

void MatchDidOpenEntry(ServiceWorkerFetchRequest* request,
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

  HeadersCallback headers_callback = base::Bind(MatchDidReadHeaderData,
                                                request,
                                                callback,
                                                blob_storage,
                                                base::Passed(entry.Pass()));

  ReadHeaders(tmp_entry_ptr, headers_callback);
}

void MatchDidReadHeaderData(
    ServiceWorkerFetchRequest* request,
    const ServiceWorkerCache::ResponseCallback& callback,
    base::WeakPtr<storage::BlobStorageContext> blob_storage,
    disk_cache::ScopedEntryPtr entry,
    scoped_ptr<ServiceWorkerRequestResponseHeaders> headers) {
  if (!headers) {
    callback.Run(ServiceWorkerCache::ErrorTypeStorage,
                 scoped_ptr<ServiceWorkerResponse>(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  scoped_ptr<ServiceWorkerResponse> response(
      new ServiceWorkerResponse(request->url,
                                headers->status_code(),
                                headers->status_text(),
                                std::map<std::string, std::string>(),
                                ""));

  for (int i = 0; i < headers->response_headers_size(); ++i) {
    const ServiceWorkerRequestResponseHeaders::HeaderMap header =
        headers->response_headers(i);
    response->headers.insert(std::make_pair(header.name(), header.value()));
  }

  // TODO(jkarlin): Insert vary validation here.

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
                 request,
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
    ServiceWorkerFetchRequest* request,
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
    MatchDoneWithBody(request,
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
                 request,
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

void MatchDoneWithBody(ServiceWorkerFetchRequest* request,
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

void DeleteDidOpenEntry(ServiceWorkerFetchRequest* request,
                        const ServiceWorkerCache::ErrorCallback& callback,
                        scoped_ptr<disk_cache::Entry*> entryptr,
                        int rv) {
  if (rv != net::OK) {
    callback.Run(ServiceWorkerCache::ErrorTypeNotFound);
    return;
  }

  DCHECK(entryptr);
  disk_cache::ScopedEntryPtr entry(*entryptr);

  entry->Doom();
  callback.Run(ServiceWorkerCache::ErrorTypeOK);
}

void ReadHeaders(disk_cache::Entry* entry, const HeadersCallback& callback) {
  DCHECK(entry);

  scoped_refptr<net::IOBufferWithSize> buffer(
      new net::IOBufferWithSize(entry->GetDataSize(INDEX_HEADERS)));

  net::CompletionCallback read_header_callback =
      base::Bind(ReadHeadersDidReadHeaderData, entry, callback, buffer);

  int read_rv = entry->ReadData(
      INDEX_HEADERS, 0, buffer.get(), buffer->size(), read_header_callback);

  if (read_rv != net::ERR_IO_PENDING)
    read_header_callback.Run(read_rv);
}

void ReadHeadersDidReadHeaderData(
    disk_cache::Entry* entry,
    const HeadersCallback& callback,
    const scoped_refptr<net::IOBufferWithSize>& buffer,
    int rv) {
  if (rv != buffer->size()) {
    callback.Run(scoped_ptr<ServiceWorkerRequestResponseHeaders>());
    return;
  }

  scoped_ptr<ServiceWorkerRequestResponseHeaders> headers(
      new ServiceWorkerRequestResponseHeaders());

  if (!headers->ParseFromArray(buffer->data(), buffer->size())) {
    callback.Run(scoped_ptr<ServiceWorkerRequestResponseHeaders>());
    return;
  }

  callback.Run(headers.Pass());
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

// The state needed to pass between ServiceWorkerCache::Keys callbacks.
struct ServiceWorkerCache::KeysContext {
  KeysContext(const ServiceWorkerCache::RequestsCallback& callback,
              base::WeakPtr<ServiceWorkerCache> cache)
      : original_callback(callback),
        cache(cache),
        out_keys(new ServiceWorkerCache::Requests()),
        backend_iterator(NULL),
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
  void* backend_iterator;
  disk_cache::Entry* enumerated_entry;
};

// static
scoped_ptr<ServiceWorkerCache> ServiceWorkerCache::CreateMemoryCache(
    net::URLRequestContext* request_context,
    base::WeakPtr<storage::BlobStorageContext> blob_context) {
  return make_scoped_ptr(
      new ServiceWorkerCache(base::FilePath(), request_context, blob_context));
}

// static
scoped_ptr<ServiceWorkerCache> ServiceWorkerCache::CreatePersistentCache(
    const base::FilePath& path,
    net::URLRequestContext* request_context,
    base::WeakPtr<storage::BlobStorageContext> blob_context) {
  return make_scoped_ptr(
      new ServiceWorkerCache(path, request_context, blob_context));
}

ServiceWorkerCache::~ServiceWorkerCache() {
}

base::WeakPtr<ServiceWorkerCache> ServiceWorkerCache::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ServiceWorkerCache::CreateBackend(const ErrorCallback& callback) {
  DCHECK(!backend_);

  // Use APP_CACHE as opposed to DISK_CACHE to prevent cache eviction.
  net::CacheType cache_type =
      path_.empty() ? net::MEMORY_CACHE : net::APP_CACHE;

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
  // TODO(jkarlin): Switch to SimpleCache after it supports APP_CACHE and after
  // debugging why the QuickStressBody unittest fails with it.
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

void ServiceWorkerCache::Put(ServiceWorkerFetchRequest* request,
                             ServiceWorkerResponse* response,
                             const ErrorCallback& callback) {
  DCHECK(backend_);

  scoped_ptr<disk_cache::Entry*> entry(new disk_cache::Entry*);

  disk_cache::Entry** entry_ptr = entry.get();

  scoped_ptr<storage::BlobDataHandle> blob_data_handle;

  if (!response->blob_uuid.empty()) {
    if (!blob_storage_context_) {
      callback.Run(ErrorTypeStorage);
      return;
    }
    blob_data_handle =
        blob_storage_context_->GetBlobDataFromUUID(response->blob_uuid);
    if (!blob_data_handle) {
      callback.Run(ErrorTypeStorage);
      return;
    }
  }

  net::CompletionCallback create_entry_callback =
      base::Bind(PutDidCreateEntry,
                 request,
                 response,
                 callback,
                 base::Passed(entry.Pass()),
                 base::Passed(blob_data_handle.Pass()),
                 request_context_);

  int rv = backend_->CreateEntry(
      request->url.spec(), entry_ptr, create_entry_callback);

  if (rv != net::ERR_IO_PENDING)
    create_entry_callback.Run(rv);
}

void ServiceWorkerCache::Match(ServiceWorkerFetchRequest* request,
                               const ResponseCallback& callback) {
  DCHECK(backend_);

  scoped_ptr<disk_cache::Entry*> entry(new disk_cache::Entry*);

  disk_cache::Entry** entry_ptr = entry.get();

  net::CompletionCallback open_entry_callback =
      base::Bind(MatchDidOpenEntry,
                 request,
                 callback,
                 blob_storage_context_,
                 base::Passed(entry.Pass()));

  int rv =
      backend_->OpenEntry(request->url.spec(), entry_ptr, open_entry_callback);
  if (rv != net::ERR_IO_PENDING)
    open_entry_callback.Run(rv);
}

void ServiceWorkerCache::Delete(ServiceWorkerFetchRequest* request,
                                const ErrorCallback& callback) {
  DCHECK(backend_);

  scoped_ptr<disk_cache::Entry*> entry(new disk_cache::Entry*);

  disk_cache::Entry** entry_ptr = entry.get();

  net::CompletionCallback open_entry_callback = base::Bind(
      DeleteDidOpenEntry, request, callback, base::Passed(entry.Pass()));

  int rv =
      backend_->OpenEntry(request->url.spec(), entry_ptr, open_entry_callback);
  if (rv != net::ERR_IO_PENDING)
    open_entry_callback.Run(rv);
}

void ServiceWorkerCache::Keys(const RequestsCallback& callback) {
  DCHECK(backend_);

  // 1. Iterate through all of the entries, open them, and add them to a vector.
  // 2. For each open entry:
  //  2.1. Read the headers into a protobuf.
  //  2.2. Copy the protobuf into a ServiceWorkerFetchRequest (a "key").
  //  2.3. Push the response into a vector of requests to be returned.
  // 3. Return the vector of requests (keys).

  // The entries have to be loaded into a vector first because enumeration loops
  // forever if you read data from a cache entry while enumerating.

  scoped_ptr<KeysContext> keys_context(
      new KeysContext(callback, weak_ptr_factory_.GetWeakPtr()));

  void** backend_iterator = &keys_context->backend_iterator;
  disk_cache::Entry** enumerated_entry = &keys_context->enumerated_entry;

  net::CompletionCallback open_entry_callback =
      base::Bind(KeysDidOpenNextEntry, base::Passed(keys_context.Pass()));

  int rv = backend_->OpenNextEntry(
      backend_iterator, enumerated_entry, open_entry_callback);

  if (rv != net::ERR_IO_PENDING)
    open_entry_callback.Run(rv);
}

bool ServiceWorkerCache::HasCreatedBackend() const {
  return backend_;
}

ServiceWorkerCache::ServiceWorkerCache(
    const base::FilePath& path,
    net::URLRequestContext* request_context,
    base::WeakPtr<storage::BlobStorageContext> blob_context)
    : path_(path),
      request_context_(request_context),
      blob_storage_context_(blob_context),
      weak_ptr_factory_(this) {
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

  // Store the entry.
  keys_context->entries.push_back(keys_context->enumerated_entry);
  keys_context->enumerated_entry = NULL;

  // Enumerate the next entry.
  void** backend_iterator = &keys_context->backend_iterator;
  disk_cache::Entry** enumerated_entry = &keys_context->enumerated_entry;

  net::CompletionCallback open_entry_callback =
      base::Bind(KeysDidOpenNextEntry, base::Passed(keys_context.Pass()));

  rv = cache->backend_->OpenNextEntry(
      backend_iterator, enumerated_entry, open_entry_callback);

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

  ReadHeaders(
      *iter,
      base::Bind(KeysDidReadHeaders, base::Passed(keys_context.Pass()), iter));
}

// static
void ServiceWorkerCache::KeysDidReadHeaders(
    scoped_ptr<KeysContext> keys_context,
    const Entries::iterator& iter,
    scoped_ptr<ServiceWorkerRequestResponseHeaders> headers) {
  disk_cache::Entry* entry = *iter;

  if (headers) {
    keys_context->out_keys->push_back(
        ServiceWorkerFetchRequest(GURL(entry->GetKey()),
                                  headers->method(),
                                  std::map<std::string, std::string>(),
                                  GURL(),
                                  false));

    std::map<std::string, std::string>& req_headers =
        keys_context->out_keys->back().headers;

    for (int i = 0; i < headers->request_headers_size(); ++i) {
      const ServiceWorkerRequestResponseHeaders::HeaderMap header =
          headers->request_headers(i);
      req_headers.insert(std::make_pair(header.name(), header.value()));
    }
  } else {
    entry->Doom();
  }

  KeysProcessNextEntry(keys_context.Pass(), iter + 1);
}

}  // namespace content
