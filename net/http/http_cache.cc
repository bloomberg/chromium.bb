// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache.h"

#include <algorithm>

#include "base/compiler_specific.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/profiler/scoped_tracker.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/worker_pool.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "net/base/cache_type.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/base/upload_data_stream.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/disk_based_cert_cache.h"
#include "net/http/disk_cache_based_quic_server_info.h"
#include "net/http/http_cache_transaction.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/quic/crypto/quic_server_info.h"

namespace {

bool UseCertCache() {
  return base::FieldTrialList::FindFullName("CertCacheTrial") ==
         "ExperimentGroup";
}

// Adaptor to delete a file on a worker thread.
void DeletePath(base::FilePath path) {
  base::DeleteFile(path, false);
}

}  // namespace

namespace net {

HttpCache::DefaultBackend::DefaultBackend(
    CacheType type,
    BackendType backend_type,
    const base::FilePath& path,
    int max_bytes,
    const scoped_refptr<base::SingleThreadTaskRunner>& thread)
    : type_(type),
      backend_type_(backend_type),
      path_(path),
      max_bytes_(max_bytes),
      thread_(thread) {
}

HttpCache::DefaultBackend::~DefaultBackend() {}

// static
HttpCache::BackendFactory* HttpCache::DefaultBackend::InMemory(int max_bytes) {
  return new DefaultBackend(MEMORY_CACHE, net::CACHE_BACKEND_DEFAULT,
                            base::FilePath(), max_bytes, NULL);
}

int HttpCache::DefaultBackend::CreateBackend(
    NetLog* net_log, scoped_ptr<disk_cache::Backend>* backend,
    const CompletionCallback& callback) {
  DCHECK_GE(max_bytes_, 0);
  return disk_cache::CreateCacheBackend(type_,
                                        backend_type_,
                                        path_,
                                        max_bytes_,
                                        true,
                                        thread_,
                                        net_log,
                                        backend,
                                        callback);
}

//-----------------------------------------------------------------------------

HttpCache::ActiveEntry::ActiveEntry(disk_cache::Entry* entry)
    : disk_entry(entry),
      writer(NULL),
      will_process_pending_queue(false),
      doomed(false) {
}

HttpCache::ActiveEntry::~ActiveEntry() {
  if (disk_entry) {
    disk_entry->Close();
    disk_entry = NULL;
  }
}

//-----------------------------------------------------------------------------

// This structure keeps track of work items that are attempting to create or
// open cache entries or the backend itself.
struct HttpCache::PendingOp {
  PendingOp() : disk_entry(NULL), writer(NULL) {}
  ~PendingOp() {}

  disk_cache::Entry* disk_entry;
  scoped_ptr<disk_cache::Backend> backend;
  WorkItem* writer;
  CompletionCallback callback;  // BackendCallback.
  WorkItemList pending_queue;
};

//-----------------------------------------------------------------------------

// The type of operation represented by a work item.
enum WorkItemOperation {
  WI_CREATE_BACKEND,
  WI_OPEN_ENTRY,
  WI_CREATE_ENTRY,
  WI_DOOM_ENTRY
};

// A work item encapsulates a single request to the backend with all the
// information needed to complete that request.
class HttpCache::WorkItem {
 public:
  WorkItem(WorkItemOperation operation, Transaction* trans, ActiveEntry** entry)
      : operation_(operation),
        trans_(trans),
        entry_(entry),
        backend_(NULL) {}
  WorkItem(WorkItemOperation operation, Transaction* trans,
           const net::CompletionCallback& cb, disk_cache::Backend** backend)
      : operation_(operation),
        trans_(trans),
        entry_(NULL),
        callback_(cb),
        backend_(backend) {}
  ~WorkItem() {}

  // Calls back the transaction with the result of the operation.
  void NotifyTransaction(int result, ActiveEntry* entry) {
    DCHECK(!entry || entry->disk_entry);
    if (entry_)
      *entry_ = entry;
    if (trans_)
      trans_->io_callback().Run(result);
  }

  // Notifies the caller about the operation completion. Returns true if the
  // callback was invoked.
  bool DoCallback(int result, disk_cache::Backend* backend) {
    if (backend_)
      *backend_ = backend;
    if (!callback_.is_null()) {
      callback_.Run(result);
      return true;
    }
    return false;
  }

  WorkItemOperation operation() { return operation_; }
  void ClearTransaction() { trans_ = NULL; }
  void ClearEntry() { entry_ = NULL; }
  void ClearCallback() { callback_.Reset(); }
  bool Matches(Transaction* trans) const { return trans == trans_; }
  bool IsValid() const { return trans_ || entry_ || !callback_.is_null(); }

 private:
  WorkItemOperation operation_;
  Transaction* trans_;
  ActiveEntry** entry_;
  net::CompletionCallback callback_;  // User callback.
  disk_cache::Backend** backend_;
};

//-----------------------------------------------------------------------------

// This class encapsulates a transaction whose only purpose is to write metadata
// to a given entry.
class HttpCache::MetadataWriter {
 public:
  explicit MetadataWriter(HttpCache::Transaction* trans)
      : transaction_(trans),
        verified_(false),
        buf_len_(0) {
  }

  ~MetadataWriter() {}

  // Implements the bulk of HttpCache::WriteMetadata.
  void Write(const GURL& url,
             base::Time expected_response_time,
             IOBuffer* buf,
             int buf_len);

 private:
  void VerifyResponse(int result);
  void SelfDestroy();
  void OnIOComplete(int result);

  scoped_ptr<HttpCache::Transaction> transaction_;
  bool verified_;
  scoped_refptr<IOBuffer> buf_;
  int buf_len_;
  base::Time expected_response_time_;
  HttpRequestInfo request_info_;
  DISALLOW_COPY_AND_ASSIGN(MetadataWriter);
};

void HttpCache::MetadataWriter::Write(const GURL& url,
                                      base::Time expected_response_time,
                                      IOBuffer* buf,
                                      int buf_len) {
  DCHECK_GT(buf_len, 0);
  DCHECK(buf);
  DCHECK(buf->data());
  request_info_.url = url;
  request_info_.method = "GET";
  request_info_.load_flags = LOAD_ONLY_FROM_CACHE;

  expected_response_time_ = expected_response_time;
  buf_ = buf;
  buf_len_ = buf_len;
  verified_ = false;

  int rv = transaction_->Start(
      &request_info_,
      base::Bind(&MetadataWriter::OnIOComplete, base::Unretained(this)),
      BoundNetLog());
  if (rv != ERR_IO_PENDING)
    VerifyResponse(rv);
}

void HttpCache::MetadataWriter::VerifyResponse(int result) {
  verified_ = true;
  if (result != OK)
    return SelfDestroy();

  const HttpResponseInfo* response_info = transaction_->GetResponseInfo();
  DCHECK(response_info->was_cached);
  if (response_info->response_time != expected_response_time_)
    return SelfDestroy();

  result = transaction_->WriteMetadata(
      buf_.get(),
      buf_len_,
      base::Bind(&MetadataWriter::OnIOComplete, base::Unretained(this)));
  if (result != ERR_IO_PENDING)
    SelfDestroy();
}

void HttpCache::MetadataWriter::SelfDestroy() {
  delete this;
}

void HttpCache::MetadataWriter::OnIOComplete(int result) {
  if (!verified_)
    return VerifyResponse(result);
  SelfDestroy();
}

//-----------------------------------------------------------------------------

class HttpCache::QuicServerInfoFactoryAdaptor : public QuicServerInfoFactory {
 public:
  explicit QuicServerInfoFactoryAdaptor(HttpCache* http_cache)
      : http_cache_(http_cache) {
  }

  QuicServerInfo* GetForServer(const QuicServerId& server_id) override {
    return new DiskCacheBasedQuicServerInfo(server_id, http_cache_);
  }

 private:
  HttpCache* const http_cache_;
};

//-----------------------------------------------------------------------------

class HttpCache::AsyncValidation {
 public:
  AsyncValidation(const HttpRequestInfo& original_request, HttpCache* cache)
      : request_(original_request), cache_(cache) {}
  ~AsyncValidation() {}

  void Start(const BoundNetLog& net_log,
             scoped_ptr<Transaction> transaction,
             NetworkDelegate* network_delegate);

 private:
  void OnStarted(int result);
  void DoRead();
  void OnRead(int result);

  // Terminate this request with net error code |result|. Logs the transaction
  // result and asks HttpCache to delete this object.
  // If there was a client or server certificate error, it cannot be recovered
  // asynchronously, so we need to prevent future attempts to asynchronously
  // fetch the resource. In this case, the cache entry is doomed.
  void Terminate(int result);

  HttpRequestInfo request_;
  scoped_refptr<IOBuffer> buf_;
  CompletionCallback read_callback_;
  scoped_ptr<Transaction> transaction_;
  base::Time start_time_;

  // The HttpCache object owns this object. This object is always deleted before
  // the pointer to the cache becomes invalid.
  HttpCache* cache_;

  DISALLOW_COPY_AND_ASSIGN(AsyncValidation);
};

void HttpCache::AsyncValidation::Start(const BoundNetLog& net_log,
                                       scoped_ptr<Transaction> transaction,
                                       NetworkDelegate* network_delegate) {
  transaction_ = transaction.Pass();
  if (network_delegate) {
    // This code is necessary to enable async transactions to pass over the
    // data-reduction proxy. This is a violation of the "once-and-only-once"
    // principle, since it copies code from URLRequestHttpJob. We cannot use the
    // original callback passed to HttpCache::Transaction by URLRequestHttpJob
    // as it will only be valid as long as the URLRequestHttpJob object is
    // alive, and that object will be deleted as soon as the synchronous request
    // completes.
    //
    // This code is also an encapsulation violation. We are exploiting the fact
    // that the |request| parameter to NotifyBeforeSendProxyHeaders() is never
    // actually used for anything, and so can be NULL.
    //
    // TODO(ricea): Do this better.
    transaction_->SetBeforeProxyHeadersSentCallback(
        base::Bind(&NetworkDelegate::NotifyBeforeSendProxyHeaders,
                   base::Unretained(network_delegate),
                   static_cast<URLRequest*>(NULL)));
    // The above use of base::Unretained is safe because the NetworkDelegate has
    // to live at least as long as the HttpNetworkSession which has to live as
    // least as long as the HttpNetworkLayer which has to live at least as long
    // this HttpCache object.
  }

  DCHECK_EQ(0, request_.load_flags & LOAD_ASYNC_REVALIDATION);
  request_.load_flags |= LOAD_ASYNC_REVALIDATION;
  start_time_ = cache_->clock()->Now();
  // This use of base::Unretained is safe because |transaction_| is owned by
  // this object.
  read_callback_ = base::Bind(&AsyncValidation::OnRead, base::Unretained(this));
  // This use of base::Unretained is safe as above.
  int rv = transaction_->Start(
      &request_,
      base::Bind(&AsyncValidation::OnStarted, base::Unretained(this)),
      net_log);

  if (rv == ERR_IO_PENDING)
    return;

  OnStarted(rv);
}

void HttpCache::AsyncValidation::OnStarted(int result) {
  if (result != OK) {
    DVLOG(1) << "Asynchronous transaction start failed for " << request_.url;
    Terminate(result);
    return;
  }

  while (transaction_->IsReadyToRestartForAuth()) {
    // This code is based on URLRequestHttpJob::RestartTransactionWithAuth,
    // however when we do this here cookies on the response will not be
    // stored. Fortunately only a tiny number of sites set cookies on 401
    // responses, and none of them use stale-while-revalidate.
    result = transaction_->RestartWithAuth(
        AuthCredentials(),
        base::Bind(&AsyncValidation::OnStarted, base::Unretained(this)));
    if (result == ERR_IO_PENDING)
      return;
    if (result != OK) {
      DVLOG(1) << "Synchronous transaction restart with auth failed for "
               << request_.url;
      Terminate(result);
      return;
    }
  }

  DoRead();
}

void HttpCache::AsyncValidation::DoRead() {
  const size_t kBufSize = 4096;
  if (!buf_.get())
    buf_ = new IOBuffer(kBufSize);

  int rv = 0;
  do {
    rv = transaction_->Read(buf_.get(), kBufSize, read_callback_);
  } while (rv > 0);

  if (rv == ERR_IO_PENDING)
    return;

  OnRead(rv);
}

void HttpCache::AsyncValidation::OnRead(int result) {
  if (result > 0) {
    DoRead();
    return;
  }
  Terminate(result);
}

void HttpCache::AsyncValidation::Terminate(int result) {
  if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED || IsCertificateError(result)) {
    // We should not attempt to access this resource asynchronously again until
    // the certificate problem has been resolved.
    // TODO(ricea): For ERR_SSL_CLIENT_AUTH_CERT_NEEDED, mark the entry as
    // requiring synchronous revalidation rather than just deleting it. Other
    // certificate errors cause the resource to be considered uncacheable
    // anyway.
    cache_->DoomEntry(transaction_->key(), transaction_.get());
  }
  base::TimeDelta duration = cache_->clock()->Now() - start_time_;
  UMA_HISTOGRAM_TIMES("HttpCache.AsyncValidationDuration", duration);
  transaction_->net_log().EndEventWithNetErrorCode(
      NetLog::TYPE_ASYNC_REVALIDATION, result);
  cache_->DeleteAsyncValidation(cache_->GenerateCacheKey(&request_));
  // |this| is deleted.
}

//-----------------------------------------------------------------------------
HttpCache::HttpCache(const net::HttpNetworkSession::Params& params,
                     BackendFactory* backend_factory)
    : net_log_(params.net_log),
      backend_factory_(backend_factory),
      building_backend_(false),
      bypass_lock_for_test_(false),
      fail_conditionalization_for_test_(false),
      use_stale_while_revalidate_(params.use_stale_while_revalidate),
      mode_(NORMAL),
      network_layer_(new HttpNetworkLayer(new HttpNetworkSession(params))),
      clock_(new base::DefaultClock()),
      weak_factory_(this) {
  SetupQuicServerInfoFactory(network_layer_->GetSession());
}


// This call doesn't change the shared |session|'s QuicServerInfoFactory because
// |session| is shared.
HttpCache::HttpCache(HttpNetworkSession* session,
                     BackendFactory* backend_factory)
    : net_log_(session->net_log()),
      backend_factory_(backend_factory),
      building_backend_(false),
      bypass_lock_for_test_(false),
      fail_conditionalization_for_test_(false),
      use_stale_while_revalidate_(session->params().use_stale_while_revalidate),
      mode_(NORMAL),
      network_layer_(new HttpNetworkLayer(session)),
      clock_(new base::DefaultClock()),
      weak_factory_(this) {
}

HttpCache::HttpCache(HttpTransactionFactory* network_layer,
                     NetLog* net_log,
                     BackendFactory* backend_factory)
    : net_log_(net_log),
      backend_factory_(backend_factory),
      building_backend_(false),
      bypass_lock_for_test_(false),
      fail_conditionalization_for_test_(false),
      use_stale_while_revalidate_(false),
      mode_(NORMAL),
      network_layer_(network_layer),
      clock_(new base::DefaultClock()),
      weak_factory_(this) {
  SetupQuicServerInfoFactory(network_layer_->GetSession());
  HttpNetworkSession* session = network_layer_->GetSession();
  if (session)
    use_stale_while_revalidate_ = session->params().use_stale_while_revalidate;
}

HttpCache::~HttpCache() {
  // Transactions should see an invalid cache after this point; otherwise they
  // could see an inconsistent object (half destroyed).
  weak_factory_.InvalidateWeakPtrs();

  // If we have any active entries remaining, then we need to deactivate them.
  // We may have some pending calls to OnProcessPendingQueue, but since those
  // won't run (due to our destruction), we can simply ignore the corresponding
  // will_process_pending_queue flag.
  while (!active_entries_.empty()) {
    ActiveEntry* entry = active_entries_.begin()->second;
    entry->will_process_pending_queue = false;
    entry->pending_queue.clear();
    entry->readers.clear();
    entry->writer = NULL;
    DeactivateEntry(entry);
  }

  STLDeleteElements(&doomed_entries_);
  STLDeleteValues(&async_validations_);

  // Before deleting pending_ops_, we have to make sure that the disk cache is
  // done with said operations, or it will attempt to use deleted data.
  cert_cache_.reset();
  disk_cache_.reset();

  PendingOpsMap::iterator pending_it = pending_ops_.begin();
  for (; pending_it != pending_ops_.end(); ++pending_it) {
    // We are not notifying the transactions about the cache going away, even
    // though they are waiting for a callback that will never fire.
    PendingOp* pending_op = pending_it->second;
    delete pending_op->writer;
    bool delete_pending_op = true;
    if (building_backend_) {
      // If we don't have a backend, when its construction finishes it will
      // deliver the callbacks.
      if (!pending_op->callback.is_null()) {
        // If not null, the callback will delete the pending operation later.
        delete_pending_op = false;
      }
    } else {
      pending_op->callback.Reset();
    }

    STLDeleteElements(&pending_op->pending_queue);
    if (delete_pending_op)
      delete pending_op;
  }
}

int HttpCache::GetBackend(disk_cache::Backend** backend,
                          const CompletionCallback& callback) {
  DCHECK(!callback.is_null());

  if (disk_cache_.get()) {
    *backend = disk_cache_.get();
    return OK;
  }

  return CreateBackend(backend, callback);
}

disk_cache::Backend* HttpCache::GetCurrentBackend() const {
  return disk_cache_.get();
}

// static
bool HttpCache::ParseResponseInfo(const char* data, int len,
                                  HttpResponseInfo* response_info,
                                  bool* response_truncated) {
  Pickle pickle(data, len);
  return response_info->InitFromPickle(pickle, response_truncated);
}

void HttpCache::WriteMetadata(const GURL& url,
                              RequestPriority priority,
                              base::Time expected_response_time,
                              IOBuffer* buf,
                              int buf_len) {
  if (!buf_len)
    return;

  // Do lazy initialization of disk cache if needed.
  if (!disk_cache_.get()) {
    // We don't care about the result.
    CreateBackend(NULL, net::CompletionCallback());
  }

  HttpCache::Transaction* trans =
      new HttpCache::Transaction(priority, this);
  MetadataWriter* writer = new MetadataWriter(trans);

  // The writer will self destruct when done.
  writer->Write(url, expected_response_time, buf, buf_len);
}

void HttpCache::CloseAllConnections() {
  HttpNetworkSession* session = GetSession();
  if (session)
    session->CloseAllConnections();
}

void HttpCache::CloseIdleConnections() {
  HttpNetworkSession* session = GetSession();
  if (session)
    session->CloseIdleConnections();
}

void HttpCache::OnExternalCacheHit(const GURL& url,
                                   const std::string& http_method) {
  if (!disk_cache_.get() || mode_ == DISABLE)
    return;

  HttpRequestInfo request_info;
  request_info.url = url;
  request_info.method = http_method;
  std::string key = GenerateCacheKey(&request_info);
  disk_cache_->OnExternalCacheHit(key);
}

void HttpCache::InitializeInfiniteCache(const base::FilePath& path) {
  if (base::FieldTrialList::FindFullName("InfiniteCache") != "Yes")
    return;
  base::WorkerPool::PostTask(FROM_HERE, base::Bind(&DeletePath, path), true);
}

int HttpCache::CreateTransaction(RequestPriority priority,
                                 scoped_ptr<HttpTransaction>* trans) {
  // Do lazy initialization of disk cache if needed.
  if (!disk_cache_.get()) {
    // We don't care about the result.
    CreateBackend(NULL, net::CompletionCallback());
  }

   HttpCache::Transaction* transaction =
      new HttpCache::Transaction(priority, this);
   if (bypass_lock_for_test_)
    transaction->BypassLockForTest();
   if (fail_conditionalization_for_test_)
     transaction->FailConditionalizationForTest();

  trans->reset(transaction);
  return OK;
}

HttpCache* HttpCache::GetCache() {
  return this;
}

HttpNetworkSession* HttpCache::GetSession() {
  return network_layer_->GetSession();
}

scoped_ptr<HttpTransactionFactory>
HttpCache::SetHttpNetworkTransactionFactoryForTesting(
    scoped_ptr<HttpTransactionFactory> new_network_layer) {
  scoped_ptr<HttpTransactionFactory> old_network_layer(network_layer_.Pass());
  network_layer_ = new_network_layer.Pass();
  return old_network_layer.Pass();
}

//-----------------------------------------------------------------------------

int HttpCache::CreateBackend(disk_cache::Backend** backend,
                             const net::CompletionCallback& callback) {
  if (!backend_factory_.get())
    return ERR_FAILED;

  building_backend_ = true;

  scoped_ptr<WorkItem> item(new WorkItem(WI_CREATE_BACKEND, NULL, callback,
                                         backend));

  // This is the only operation that we can do that is not related to any given
  // entry, so we use an empty key for it.
  PendingOp* pending_op = GetPendingOp(std::string());
  if (pending_op->writer) {
    if (!callback.is_null())
      pending_op->pending_queue.push_back(item.release());
    return ERR_IO_PENDING;
  }

  DCHECK(pending_op->pending_queue.empty());

  pending_op->writer = item.release();
  pending_op->callback = base::Bind(&HttpCache::OnPendingOpComplete,
                                    GetWeakPtr(), pending_op);

  int rv = backend_factory_->CreateBackend(net_log_, &pending_op->backend,
                                           pending_op->callback);
  if (rv != ERR_IO_PENDING) {
    pending_op->writer->ClearCallback();
    pending_op->callback.Run(rv);
  }

  return rv;
}

int HttpCache::GetBackendForTransaction(Transaction* trans) {
  if (disk_cache_.get())
    return OK;

  if (!building_backend_)
    return ERR_FAILED;

  WorkItem* item = new WorkItem(
      WI_CREATE_BACKEND, trans, net::CompletionCallback(), NULL);
  PendingOp* pending_op = GetPendingOp(std::string());
  DCHECK(pending_op->writer);
  pending_op->pending_queue.push_back(item);
  return ERR_IO_PENDING;
}

// Generate a key that can be used inside the cache.
std::string HttpCache::GenerateCacheKey(const HttpRequestInfo* request) {
  // Strip out the reference, username, and password sections of the URL.
  std::string url = HttpUtil::SpecForRequest(request->url);

  DCHECK(mode_ != DISABLE);
  if (mode_ == NORMAL) {
    // No valid URL can begin with numerals, so we should not have to worry
    // about collisions with normal URLs.
    if (request->upload_data_stream &&
        request->upload_data_stream->identifier()) {
      url.insert(0, base::StringPrintf(
          "%" PRId64 "/", request->upload_data_stream->identifier()));
    }
    return url;
  }

  // In playback and record mode, we cache everything.

  // Lazily initialize.
  if (playback_cache_map_ == NULL)
    playback_cache_map_.reset(new PlaybackCacheMap());

  // Each time we request an item from the cache, we tag it with a
  // generation number.  During playback, multiple fetches for the same
  // item will use the same generation number and pull the proper
  // instance of an URL from the cache.
  int generation = 0;
  DCHECK(playback_cache_map_ != NULL);
  if (playback_cache_map_->find(url) != playback_cache_map_->end())
    generation = (*playback_cache_map_)[url];
  (*playback_cache_map_)[url] = generation + 1;

  // The key into the cache is GENERATION # + METHOD + URL.
  std::string result = base::IntToString(generation);
  result.append(request->method);
  result.append(url);
  return result;
}

void HttpCache::DoomActiveEntry(const std::string& key) {
  ActiveEntriesMap::iterator it = active_entries_.find(key);
  if (it == active_entries_.end())
    return;

  // This is not a performance critical operation, this is handling an error
  // condition so it is OK to look up the entry again.
  int rv = DoomEntry(key, NULL);
  DCHECK_EQ(OK, rv);
}

int HttpCache::DoomEntry(const std::string& key, Transaction* trans) {
  // Need to abandon the ActiveEntry, but any transaction attached to the entry
  // should not be impacted.  Dooming an entry only means that it will no
  // longer be returned by FindActiveEntry (and it will also be destroyed once
  // all consumers are finished with the entry).
  ActiveEntriesMap::iterator it = active_entries_.find(key);
  if (it == active_entries_.end()) {
    DCHECK(trans);
    return AsyncDoomEntry(key, trans);
  }

  ActiveEntry* entry = it->second;
  active_entries_.erase(it);

  // We keep track of doomed entries so that we can ensure that they are
  // cleaned up properly when the cache is destroyed.
  doomed_entries_.insert(entry);

  entry->disk_entry->Doom();
  entry->doomed = true;

  DCHECK(entry->writer || !entry->readers.empty() ||
         entry->will_process_pending_queue);
  return OK;
}

int HttpCache::AsyncDoomEntry(const std::string& key, Transaction* trans) {
  WorkItem* item = new WorkItem(WI_DOOM_ENTRY, trans, NULL);
  PendingOp* pending_op = GetPendingOp(key);
  if (pending_op->writer) {
    pending_op->pending_queue.push_back(item);
    return ERR_IO_PENDING;
  }

  DCHECK(pending_op->pending_queue.empty());

  pending_op->writer = item;
  pending_op->callback = base::Bind(&HttpCache::OnPendingOpComplete,
                                    GetWeakPtr(), pending_op);

  int rv = disk_cache_->DoomEntry(key, pending_op->callback);
  if (rv != ERR_IO_PENDING) {
    item->ClearTransaction();
    pending_op->callback.Run(rv);
  }

  return rv;
}

void HttpCache::DoomMainEntryForUrl(const GURL& url) {
  if (!disk_cache_)
    return;

  HttpRequestInfo temp_info;
  temp_info.url = url;
  temp_info.method = "GET";
  std::string key = GenerateCacheKey(&temp_info);

  // Defer to DoomEntry if there is an active entry, otherwise call
  // AsyncDoomEntry without triggering a callback.
  if (active_entries_.count(key))
    DoomEntry(key, NULL);
  else
    AsyncDoomEntry(key, NULL);
}

void HttpCache::FinalizeDoomedEntry(ActiveEntry* entry) {
  DCHECK(entry->doomed);
  DCHECK(!entry->writer);
  DCHECK(entry->readers.empty());
  DCHECK(entry->pending_queue.empty());

  ActiveEntriesSet::iterator it = doomed_entries_.find(entry);
  DCHECK(it != doomed_entries_.end());
  doomed_entries_.erase(it);

  delete entry;
}

HttpCache::ActiveEntry* HttpCache::FindActiveEntry(const std::string& key) {
  ActiveEntriesMap::const_iterator it = active_entries_.find(key);
  return it != active_entries_.end() ? it->second : NULL;
}

HttpCache::ActiveEntry* HttpCache::ActivateEntry(
    disk_cache::Entry* disk_entry) {
  DCHECK(!FindActiveEntry(disk_entry->GetKey()));
  ActiveEntry* entry = new ActiveEntry(disk_entry);
  active_entries_[disk_entry->GetKey()] = entry;
  return entry;
}

void HttpCache::DeactivateEntry(ActiveEntry* entry) {
  DCHECK(!entry->will_process_pending_queue);
  DCHECK(!entry->doomed);
  DCHECK(!entry->writer);
  DCHECK(entry->disk_entry);
  DCHECK(entry->readers.empty());
  DCHECK(entry->pending_queue.empty());

  std::string key = entry->disk_entry->GetKey();
  if (key.empty())
    return SlowDeactivateEntry(entry);

  ActiveEntriesMap::iterator it = active_entries_.find(key);
  DCHECK(it != active_entries_.end());
  DCHECK(it->second == entry);

  active_entries_.erase(it);
  delete entry;
}

// We don't know this entry's key so we have to find it without it.
void HttpCache::SlowDeactivateEntry(ActiveEntry* entry) {
  for (ActiveEntriesMap::iterator it = active_entries_.begin();
       it != active_entries_.end(); ++it) {
    if (it->second == entry) {
      active_entries_.erase(it);
      delete entry;
      break;
    }
  }
}

HttpCache::PendingOp* HttpCache::GetPendingOp(const std::string& key) {
  DCHECK(!FindActiveEntry(key));

  PendingOpsMap::const_iterator it = pending_ops_.find(key);
  if (it != pending_ops_.end())
    return it->second;

  PendingOp* operation = new PendingOp();
  pending_ops_[key] = operation;
  return operation;
}

void HttpCache::DeletePendingOp(PendingOp* pending_op) {
  std::string key;
  if (pending_op->disk_entry)
    key = pending_op->disk_entry->GetKey();

  if (!key.empty()) {
    PendingOpsMap::iterator it = pending_ops_.find(key);
    DCHECK(it != pending_ops_.end());
    pending_ops_.erase(it);
  } else {
    for (PendingOpsMap::iterator it = pending_ops_.begin();
         it != pending_ops_.end(); ++it) {
      if (it->second == pending_op) {
        pending_ops_.erase(it);
        break;
      }
    }
  }
  DCHECK(pending_op->pending_queue.empty());

  delete pending_op;
}

int HttpCache::OpenEntry(const std::string& key, ActiveEntry** entry,
                         Transaction* trans) {
  ActiveEntry* active_entry = FindActiveEntry(key);
  if (active_entry) {
    *entry = active_entry;
    return OK;
  }

  WorkItem* item = new WorkItem(WI_OPEN_ENTRY, trans, entry);
  PendingOp* pending_op = GetPendingOp(key);
  if (pending_op->writer) {
    pending_op->pending_queue.push_back(item);
    return ERR_IO_PENDING;
  }

  DCHECK(pending_op->pending_queue.empty());

  pending_op->writer = item;
  pending_op->callback = base::Bind(&HttpCache::OnPendingOpComplete,
                                    GetWeakPtr(), pending_op);

  int rv = disk_cache_->OpenEntry(key, &(pending_op->disk_entry),
                                  pending_op->callback);
  if (rv != ERR_IO_PENDING) {
    item->ClearTransaction();
    pending_op->callback.Run(rv);
  }

  return rv;
}

int HttpCache::CreateEntry(const std::string& key, ActiveEntry** entry,
                           Transaction* trans) {
  if (FindActiveEntry(key)) {
    return ERR_CACHE_RACE;
  }

  WorkItem* item = new WorkItem(WI_CREATE_ENTRY, trans, entry);
  PendingOp* pending_op = GetPendingOp(key);
  if (pending_op->writer) {
    pending_op->pending_queue.push_back(item);
    return ERR_IO_PENDING;
  }

  DCHECK(pending_op->pending_queue.empty());

  pending_op->writer = item;
  pending_op->callback = base::Bind(&HttpCache::OnPendingOpComplete,
                                    GetWeakPtr(), pending_op);

  int rv = disk_cache_->CreateEntry(key, &(pending_op->disk_entry),
                                    pending_op->callback);
  if (rv != ERR_IO_PENDING) {
    item->ClearTransaction();
    pending_op->callback.Run(rv);
  }

  return rv;
}

void HttpCache::DestroyEntry(ActiveEntry* entry) {
  if (entry->doomed) {
    FinalizeDoomedEntry(entry);
  } else {
    DeactivateEntry(entry);
  }
}

int HttpCache::AddTransactionToEntry(ActiveEntry* entry, Transaction* trans) {
  DCHECK(entry);
  DCHECK(entry->disk_entry);

  // We implement a basic reader/writer lock for the disk cache entry.  If
  // there is already a writer, then everyone has to wait for the writer to
  // finish before they can access the cache entry.  There can be multiple
  // readers.
  //
  // NOTE: If the transaction can only write, then the entry should not be in
  // use (since any existing entry should have already been doomed).

  if (entry->writer || entry->will_process_pending_queue) {
    entry->pending_queue.push_back(trans);
    return ERR_IO_PENDING;
  }

  if (trans->mode() & Transaction::WRITE) {
    // transaction needs exclusive access to the entry
    if (entry->readers.empty()) {
      entry->writer = trans;
    } else {
      entry->pending_queue.push_back(trans);
      return ERR_IO_PENDING;
    }
  } else {
    // transaction needs read access to the entry
    entry->readers.push_back(trans);
  }

  // We do this before calling EntryAvailable to force any further calls to
  // AddTransactionToEntry to add their transaction to the pending queue, which
  // ensures FIFO ordering.
  if (!entry->writer && !entry->pending_queue.empty())
    ProcessPendingQueue(entry);

  return OK;
}

void HttpCache::DoneWithEntry(ActiveEntry* entry, Transaction* trans,
                              bool cancel) {
  // If we already posted a task to move on to the next transaction and this was
  // the writer, there is nothing to cancel.
  if (entry->will_process_pending_queue && entry->readers.empty())
    return;

  if (entry->writer) {
    DCHECK(trans == entry->writer);

    // Assume there was a failure.
    bool success = false;
    if (cancel) {
      DCHECK(entry->disk_entry);
      // This is a successful operation in the sense that we want to keep the
      // entry.
      success = trans->AddTruncatedFlag();
      // The previous operation may have deleted the entry.
      if (!trans->entry())
        return;
    }
    DoneWritingToEntry(entry, success);
  } else {
    DoneReadingFromEntry(entry, trans);
  }
}

void HttpCache::DoneWritingToEntry(ActiveEntry* entry, bool success) {
  DCHECK(entry->readers.empty());

  entry->writer = NULL;

  if (success) {
    ProcessPendingQueue(entry);
  } else {
    DCHECK(!entry->will_process_pending_queue);

    // We failed to create this entry.
    TransactionList pending_queue;
    pending_queue.swap(entry->pending_queue);

    entry->disk_entry->Doom();
    DestroyEntry(entry);

    // We need to do something about these pending entries, which now need to
    // be added to a new entry.
    while (!pending_queue.empty()) {
      // ERR_CACHE_RACE causes the transaction to restart the whole process.
      pending_queue.front()->io_callback().Run(ERR_CACHE_RACE);
      pending_queue.pop_front();
    }
  }
}

void HttpCache::DoneReadingFromEntry(ActiveEntry* entry, Transaction* trans) {
  DCHECK(!entry->writer);

  TransactionList::iterator it =
      std::find(entry->readers.begin(), entry->readers.end(), trans);
  DCHECK(it != entry->readers.end());

  entry->readers.erase(it);

  ProcessPendingQueue(entry);
}

void HttpCache::ConvertWriterToReader(ActiveEntry* entry) {
  DCHECK(entry->writer);
  DCHECK(entry->writer->mode() == Transaction::READ_WRITE);
  DCHECK(entry->readers.empty());

  Transaction* trans = entry->writer;

  entry->writer = NULL;
  entry->readers.push_back(trans);

  ProcessPendingQueue(entry);
}

LoadState HttpCache::GetLoadStateForPendingTransaction(
      const Transaction* trans) {
  ActiveEntriesMap::const_iterator i = active_entries_.find(trans->key());
  if (i == active_entries_.end()) {
    // If this is really a pending transaction, and it is not part of
    // active_entries_, we should be creating the backend or the entry.
    return LOAD_STATE_WAITING_FOR_CACHE;
  }

  Transaction* writer = i->second->writer;
  return writer ? writer->GetWriterLoadState() : LOAD_STATE_WAITING_FOR_CACHE;
}

void HttpCache::RemovePendingTransaction(Transaction* trans) {
  ActiveEntriesMap::const_iterator i = active_entries_.find(trans->key());
  bool found = false;
  if (i != active_entries_.end())
    found = RemovePendingTransactionFromEntry(i->second, trans);

  if (found)
    return;

  if (building_backend_) {
    PendingOpsMap::const_iterator j = pending_ops_.find(std::string());
    if (j != pending_ops_.end())
      found = RemovePendingTransactionFromPendingOp(j->second, trans);

    if (found)
      return;
  }

  PendingOpsMap::const_iterator j = pending_ops_.find(trans->key());
  if (j != pending_ops_.end())
    found = RemovePendingTransactionFromPendingOp(j->second, trans);

  if (found)
    return;

  ActiveEntriesSet::iterator k = doomed_entries_.begin();
  for (; k != doomed_entries_.end() && !found; ++k)
    found = RemovePendingTransactionFromEntry(*k, trans);

  DCHECK(found) << "Pending transaction not found";
}

bool HttpCache::RemovePendingTransactionFromEntry(ActiveEntry* entry,
                                                  Transaction* trans) {
  TransactionList& pending_queue = entry->pending_queue;

  TransactionList::iterator j =
      find(pending_queue.begin(), pending_queue.end(), trans);
  if (j == pending_queue.end())
    return false;

  pending_queue.erase(j);
  return true;
}

bool HttpCache::RemovePendingTransactionFromPendingOp(PendingOp* pending_op,
                                                      Transaction* trans) {
  if (pending_op->writer->Matches(trans)) {
    pending_op->writer->ClearTransaction();
    pending_op->writer->ClearEntry();
    return true;
  }
  WorkItemList& pending_queue = pending_op->pending_queue;

  WorkItemList::iterator it = pending_queue.begin();
  for (; it != pending_queue.end(); ++it) {
    if ((*it)->Matches(trans)) {
      delete *it;
      pending_queue.erase(it);
      return true;
    }
  }
  return false;
}

void HttpCache::SetupQuicServerInfoFactory(HttpNetworkSession* session) {
  if (session &&
      !session->quic_stream_factory()->has_quic_server_info_factory()) {
    DCHECK(!quic_server_info_factory_);
    quic_server_info_factory_.reset(new QuicServerInfoFactoryAdaptor(this));
    session->quic_stream_factory()->set_quic_server_info_factory(
        quic_server_info_factory_.get());
  }
}

void HttpCache::ProcessPendingQueue(ActiveEntry* entry) {
  // Multiple readers may finish with an entry at once, so we want to batch up
  // calls to OnProcessPendingQueue.  This flag also tells us that we should
  // not delete the entry before OnProcessPendingQueue runs.
  if (entry->will_process_pending_queue)
    return;
  entry->will_process_pending_queue = true;

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&HttpCache::OnProcessPendingQueue, GetWeakPtr(), entry));
}

void HttpCache::PerformAsyncValidation(const HttpRequestInfo& original_request,
                                       const BoundNetLog& net_log) {
  DCHECK(use_stale_while_revalidate_);
  std::string key = GenerateCacheKey(&original_request);
  AsyncValidation* async_validation =
      new AsyncValidation(original_request, this);
  typedef AsyncValidationMap::value_type AsyncValidationKeyValue;
  bool insert_ok =
      async_validations_.insert(AsyncValidationKeyValue(key, async_validation))
          .second;
  if (!insert_ok) {
    DVLOG(1) << "Harmless race condition detected on URL "
             << original_request.url << "; discarding redundant revalidation.";
    delete async_validation;
    return;
  }
  HttpNetworkSession* network_session = GetSession();
  NetworkDelegate* network_delegate = NULL;
  if (network_session)
    network_delegate = network_session->network_delegate();
  scoped_ptr<HttpTransaction> transaction;
  CreateTransaction(IDLE, &transaction);
  scoped_ptr<Transaction> downcast_transaction(
      static_cast<Transaction*>(transaction.release()));
  async_validation->Start(
      net_log, downcast_transaction.Pass(), network_delegate);
  // |async_validation| may have been deleted here.
}

void HttpCache::DeleteAsyncValidation(const std::string& url) {
  AsyncValidationMap::iterator it = async_validations_.find(url);
  CHECK(it != async_validations_.end());  // security-critical invariant
  AsyncValidation* async_validation = it->second;
  async_validations_.erase(it);
  delete async_validation;
}

void HttpCache::OnProcessPendingQueue(ActiveEntry* entry) {
  entry->will_process_pending_queue = false;
  DCHECK(!entry->writer);

  // If no one is interested in this entry, then we can deactivate it.
  if (entry->pending_queue.empty()) {
    if (entry->readers.empty())
      DestroyEntry(entry);
    return;
  }

  // Promote next transaction from the pending queue.
  Transaction* next = entry->pending_queue.front();
  if ((next->mode() & Transaction::WRITE) && !entry->readers.empty())
    return;  // Have to wait.

  entry->pending_queue.erase(entry->pending_queue.begin());

  int rv = AddTransactionToEntry(entry, next);
  if (rv != ERR_IO_PENDING) {
    next->io_callback().Run(rv);
  }
}

void HttpCache::OnIOComplete(int result, PendingOp* pending_op) {
  WorkItemOperation op = pending_op->writer->operation();

  // Completing the creation of the backend is simpler than the other cases.
  if (op == WI_CREATE_BACKEND)
    return OnBackendCreated(result, pending_op);

  scoped_ptr<WorkItem> item(pending_op->writer);
  bool fail_requests = false;

  ActiveEntry* entry = NULL;
  std::string key;
  if (result == OK) {
    if (op == WI_DOOM_ENTRY) {
      // Anything after a Doom has to be restarted.
      fail_requests = true;
    } else if (item->IsValid()) {
      key = pending_op->disk_entry->GetKey();
      entry = ActivateEntry(pending_op->disk_entry);
    } else {
      // The writer transaction is gone.
      if (op == WI_CREATE_ENTRY)
        pending_op->disk_entry->Doom();
      pending_op->disk_entry->Close();
      pending_op->disk_entry = NULL;
      fail_requests = true;
    }
  }

  // We are about to notify a bunch of transactions, and they may decide to
  // re-issue a request (or send a different one). If we don't delete
  // pending_op, the new request will be appended to the end of the list, and
  // we'll see it again from this point before it has a chance to complete (and
  // we'll be messing out the request order). The down side is that if for some
  // reason notifying request A ends up cancelling request B (for the same key),
  // we won't find request B anywhere (because it would be in a local variable
  // here) and that's bad. If there is a chance for that to happen, we'll have
  // to move the callback used to be a CancelableCallback. By the way, for this
  // to happen the action (to cancel B) has to be synchronous to the
  // notification for request A.
  WorkItemList pending_items;
  pending_items.swap(pending_op->pending_queue);
  DeletePendingOp(pending_op);

  item->NotifyTransaction(result, entry);

  while (!pending_items.empty()) {
    item.reset(pending_items.front());
    pending_items.pop_front();

    if (item->operation() == WI_DOOM_ENTRY) {
      // A queued doom request is always a race.
      fail_requests = true;
    } else if (result == OK) {
      entry = FindActiveEntry(key);
      if (!entry)
        fail_requests = true;
    }

    if (fail_requests) {
      item->NotifyTransaction(ERR_CACHE_RACE, NULL);
      continue;
    }

    if (item->operation() == WI_CREATE_ENTRY) {
      if (result == OK) {
        // A second Create request, but the first request succeeded.
        item->NotifyTransaction(ERR_CACHE_CREATE_FAILURE, NULL);
      } else {
        if (op != WI_CREATE_ENTRY) {
          // Failed Open followed by a Create.
          item->NotifyTransaction(ERR_CACHE_RACE, NULL);
          fail_requests = true;
        } else {
          item->NotifyTransaction(result, entry);
        }
      }
    } else {
      if (op == WI_CREATE_ENTRY && result != OK) {
        // Failed Create followed by an Open.
        item->NotifyTransaction(ERR_CACHE_RACE, NULL);
        fail_requests = true;
      } else {
        item->NotifyTransaction(result, entry);
      }
    }
  }
}

// static
void HttpCache::OnPendingOpComplete(const base::WeakPtr<HttpCache>& cache,
                                    PendingOp* pending_op,
                                    int rv) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/422516 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422516 HttpCache::OnPendingOpComplete"));

  if (cache.get()) {
    cache->OnIOComplete(rv, pending_op);
  } else {
    // The callback was cancelled so we should delete the pending_op that
    // was used with this callback.
    delete pending_op;
  }
}

void HttpCache::OnBackendCreated(int result, PendingOp* pending_op) {
  scoped_ptr<WorkItem> item(pending_op->writer);
  WorkItemOperation op = item->operation();
  DCHECK_EQ(WI_CREATE_BACKEND, op);

  // We don't need the callback anymore.
  pending_op->callback.Reset();

  if (backend_factory_.get()) {
    // We may end up calling OnBackendCreated multiple times if we have pending
    // work items. The first call saves the backend and releases the factory,
    // and the last call clears building_backend_.
    backend_factory_.reset();  // Reclaim memory.
    if (result == OK) {
      disk_cache_ = pending_op->backend.Pass();
      if (UseCertCache())
        cert_cache_.reset(new DiskBasedCertCache(disk_cache_.get()));
    }
  }

  if (!pending_op->pending_queue.empty()) {
    WorkItem* pending_item = pending_op->pending_queue.front();
    pending_op->pending_queue.pop_front();
    DCHECK_EQ(WI_CREATE_BACKEND, pending_item->operation());

    // We want to process a single callback at a time, because the cache may
    // go away from the callback.
    pending_op->writer = pending_item;

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&HttpCache::OnBackendCreated, GetWeakPtr(),
                   result, pending_op));
  } else {
    building_backend_ = false;
    DeletePendingOp(pending_op);
  }

  // The cache may be gone when we return from the callback.
  if (!item->DoCallback(result, disk_cache_.get()))
    item->NotifyTransaction(result, NULL);
}

}  // namespace net
