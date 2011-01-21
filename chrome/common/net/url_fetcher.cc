// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/url_fetcher.h"

#include <set>

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_throttler_manager.h"

static const int kBufferSize = 4096;

bool URLFetcher::g_interception_enabled = false;

class URLFetcher::Core
    : public base::RefCountedThreadSafe<URLFetcher::Core>,
      public net::URLRequest::Delegate {
 public:
  // For POST requests, set |content_type| to the MIME type of the content
  // and set |content| to the data to upload.  |flags| are flags to apply to
  // the load operation--these should be one or more of the LOAD_* flags
  // defined in url_request.h.
  Core(URLFetcher* fetcher,
       const GURL& original_url,
       RequestType request_type,
       URLFetcher::Delegate* d);

  // Starts the load.  It's important that this not happen in the constructor
  // because it causes the IO thread to begin AddRef()ing and Release()ing
  // us.  If our caller hasn't had time to fully construct us and take a
  // reference, the IO thread could interrupt things, run a task, Release()
  // us, and destroy us, leaving the caller with an already-destroyed object
  // when construction finishes.
  void Start();

  // Stops any in-progress load and ensures no callback will happen.  It is
  // safe to call this multiple times.
  void Stop();

  // Reports that the received content was malformed.
  void ReceivedContentWasMalformed();

  // Overridden from net::URLRequest::Delegate:
  virtual void OnResponseStarted(net::URLRequest* request);
  virtual void OnReadCompleted(net::URLRequest* request, int bytes_read);

  URLFetcher::Delegate* delegate() const { return delegate_; }

  static void CancelAll();

 private:
  friend class base::RefCountedThreadSafe<URLFetcher::Core>;

  class Registry {
   public:
    Registry();
    ~Registry();

    void AddURLFetcherCore(Core* core);
    void RemoveURLFetcherCore(Core* core);

    void CancelAll();

   private:
    std::set<Core*> fetchers_;

    DISALLOW_COPY_AND_ASSIGN(Registry);
  };

  ~Core();

  // Wrapper functions that allow us to ensure actions happen on the right
  // thread.
  void StartURLRequest();
  void StartURLRequestWhenAppropriate();
  void CancelURLRequest();
  void OnCompletedURLRequest(const net::URLRequestStatus& status);
  void NotifyMalformedContent();

  // Deletes the request, removes it from the registry, and removes the
  // destruction observer.
  void ReleaseRequest();

  // Returns the max value of exponential back-off release time for
  // |original_url_| and |url_|.
  base::TimeTicks GetBackoffReleaseTime();

  URLFetcher* fetcher_;              // Corresponding fetcher object
  GURL original_url_;                // The URL we were asked to fetch
  GURL url_;                         // The URL we eventually wound up at
  RequestType request_type_;         // What type of request is this?
  URLFetcher::Delegate* delegate_;   // Object to notify on completion
  scoped_refptr<base::MessageLoopProxy> delegate_loop_proxy_;
                                     // Message loop proxy of the creating
                                     // thread.
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
                                     // The message loop proxy for the thread
                                     // on which the request IO happens.
  scoped_ptr<net::URLRequest> request_;   // The actual request this wraps
  int load_flags_;                   // Flags for the load operation
  int response_code_;                // HTTP status code for the request
  std::string data_;                 // Results of the request
  scoped_refptr<net::IOBuffer> buffer_;
                                     // Read buffer
  scoped_refptr<URLRequestContextGetter> request_context_getter_;
                                     // Cookie/cache info for the request
  ResponseCookies cookies_;          // Response cookies
  net::HttpRequestHeaders extra_request_headers_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;

  std::string upload_content_;       // HTTP POST payload
  std::string upload_content_type_;  // MIME type of POST payload
  std::string referrer_;             // HTTP Referer header value

  // Used to determine how long to wait before making a request or doing a
  // retry.
  // Both of them can only be accessed on the IO thread.
  // We need not only the throttler entry for |original_URL|, but also the one
  // for |url|. For example, consider the case that URL A redirects to URL B,
  // for which the server returns a 500 response. In this case, the exponential
  // back-off release time of URL A won't increase. If we retry without
  // considering the back-off constraint of URL B, we may send out too many
  // requests for URL A in a short period of time.
  scoped_refptr<net::URLRequestThrottlerEntryInterface>
      original_url_throttler_entry_;
  scoped_refptr<net::URLRequestThrottlerEntryInterface> url_throttler_entry_;

  // |num_retries_| indicates how many times we've failed to successfully
  // fetch this URL.  Once this value exceeds the maximum number of retries
  // specified by the owner URLFetcher instance, we'll give up.
  int num_retries_;

  // True if the URLFetcher has been cancelled.
  bool was_cancelled_;

  // Since GetBackoffReleaseTime() can only be called on the IO thread, we cache
  // its value to be used by OnCompletedURLRequest on the creating thread.
  base::TimeTicks backoff_release_time_;

  static base::LazyInstance<Registry> g_registry;

  friend class URLFetcher;
  DISALLOW_COPY_AND_ASSIGN(Core);
};

URLFetcher::Core::Registry::Registry() {}
URLFetcher::Core::Registry::~Registry() {}

void URLFetcher::Core::Registry::AddURLFetcherCore(Core* core) {
  DCHECK(!ContainsKey(fetchers_, core));
  fetchers_.insert(core);
}

void URLFetcher::Core::Registry::RemoveURLFetcherCore(Core* core) {
  DCHECK(ContainsKey(fetchers_, core));
  fetchers_.erase(core);
}

void URLFetcher::Core::Registry::CancelAll() {
  std::set<Core*> fetchers;
  fetchers.swap(fetchers_);

  for (std::set<Core*>::iterator it = fetchers.begin();
       it != fetchers.end(); ++it)
    (*it)->CancelURLRequest();
}

// static
base::LazyInstance<URLFetcher::Core::Registry>
    URLFetcher::Core::g_registry(base::LINKER_INITIALIZED);

// static
URLFetcher::Factory* URLFetcher::factory_ = NULL;

URLFetcher::URLFetcher(const GURL& url,
                       RequestType request_type,
                       Delegate* d)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
      core_(new Core(this, url, request_type, d))),
      automatically_retry_on_5xx_(true),
      max_retries_(0) {
}

URLFetcher::~URLFetcher() {
  core_->Stop();
}

// static
URLFetcher* URLFetcher::Create(int id, const GURL& url,
                               RequestType request_type, Delegate* d) {
  return factory_ ? factory_->CreateURLFetcher(id, url, request_type, d) :
                    new URLFetcher(url, request_type, d);
}

URLFetcher::Core::Core(URLFetcher* fetcher,
                       const GURL& original_url,
                       RequestType request_type,
                       URLFetcher::Delegate* d)
    : fetcher_(fetcher),
      original_url_(original_url),
      request_type_(request_type),
      delegate_(d),
      delegate_loop_proxy_(base::MessageLoopProxy::CreateForCurrentThread()),
      request_(NULL),
      load_flags_(net::LOAD_NORMAL),
      response_code_(-1),
      buffer_(new net::IOBuffer(kBufferSize)),
      num_retries_(0),
      was_cancelled_(false) {
}

URLFetcher::Core::~Core() {
  // |request_| should be NULL.  If not, it's unsafe to delete it here since we
  // may not be on the IO thread.
  DCHECK(!request_.get());
}

void URLFetcher::Core::Start() {
  DCHECK(delegate_loop_proxy_);
  CHECK(request_context_getter_) << "We need an URLRequestContext!";
  io_message_loop_proxy_ = request_context_getter_->GetIOMessageLoopProxy();
  CHECK(io_message_loop_proxy_.get()) << "We need an IO message loop proxy";

  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &Core::StartURLRequestWhenAppropriate));
}

void URLFetcher::Core::Stop() {
  DCHECK(delegate_loop_proxy_->BelongsToCurrentThread());
  delegate_ = NULL;
  fetcher_ = NULL;
  if (io_message_loop_proxy_.get()) {
    io_message_loop_proxy_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &Core::CancelURLRequest));
  }
}

void URLFetcher::Core::ReceivedContentWasMalformed() {
  DCHECK(delegate_loop_proxy_->BelongsToCurrentThread());
  if (io_message_loop_proxy_.get()) {
    io_message_loop_proxy_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &Core::NotifyMalformedContent));
  }
}

void URLFetcher::Core::CancelAll() {
  g_registry.Get().CancelAll();
}

void URLFetcher::Core::OnResponseStarted(net::URLRequest* request) {
  DCHECK_EQ(request, request_.get());
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  if (request_->status().is_success()) {
    response_code_ = request_->GetResponseCode();
    response_headers_ = request_->response_headers();
  }

  int bytes_read = 0;
  // Some servers may treat HEAD requests as GET requests.  To free up the
  // network connection as soon as possible, signal that the request has
  // completed immediately, without trying to read any data back (all we care
  // about is the response code and headers, which we already have).
  if (request_->status().is_success() && (request_type_ != HEAD))
    request_->Read(buffer_, kBufferSize, &bytes_read);
  OnReadCompleted(request_.get(), bytes_read);
}

void URLFetcher::Core::OnReadCompleted(net::URLRequest* request,
                                       int bytes_read) {
  DCHECK(request == request_);
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  url_ = request->url();
  url_throttler_entry_ =
      net::URLRequestThrottlerManager::GetInstance()->RegisterRequestUrl(url_);

  do {
    if (!request_->status().is_success() || bytes_read <= 0)
      break;
    data_.append(buffer_->data(), bytes_read);
  } while (request_->Read(buffer_, kBufferSize, &bytes_read));

  if (request_->status().is_success())
    request_->GetResponseCookies(&cookies_);

  // See comments re: HEAD requests in OnResponseStarted().
  if (!request_->status().is_io_pending() || (request_type_ == HEAD)) {
    backoff_release_time_ = GetBackoffReleaseTime();

    bool posted = delegate_loop_proxy_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &Core::OnCompletedURLRequest,
                          request_->status()));
    // If the delegate message loop does not exist any more, then the delegate
    // should be gone too.
    DCHECK(posted || !delegate_);
    ReleaseRequest();
  }
}

void URLFetcher::Core::StartURLRequest() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (was_cancelled_) {
    // Since StartURLRequest() is posted as a *delayed* task, it may
    // run after the URLFetcher was already stopped.
    return;
  }

  CHECK(request_context_getter_);
  DCHECK(!request_.get());

  g_registry.Get().AddURLFetcherCore(this);
  request_.reset(new net::URLRequest(original_url_, this));
  int flags = request_->load_flags() | load_flags_;
  if (!g_interception_enabled) {
    flags = flags | net::LOAD_DISABLE_INTERCEPT;
  }
  request_->set_load_flags(flags);
  request_->set_context(request_context_getter_->GetURLRequestContext());
  request_->set_referrer(referrer_);

  switch (request_type_) {
    case GET:
      break;

    case POST:
      DCHECK(!upload_content_.empty());
      DCHECK(!upload_content_type_.empty());

      request_->set_method("POST");
      extra_request_headers_.SetHeader(net::HttpRequestHeaders::kContentType,
                                       upload_content_type_);
      request_->AppendBytesToUpload(upload_content_.data(),
                                    static_cast<int>(upload_content_.size()));
      break;

    case HEAD:
      request_->set_method("HEAD");
      break;

    default:
      NOTREACHED();
  }

  if (!extra_request_headers_.IsEmpty())
    request_->SetExtraRequestHeaders(extra_request_headers_);

  // There might be data left over from a previous request attempt.
  data_.clear();

  request_->Start();
}

void URLFetcher::Core::StartURLRequestWhenAppropriate() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (was_cancelled_)
    return;

  if (original_url_throttler_entry_ == NULL) {
    original_url_throttler_entry_ =
        net::URLRequestThrottlerManager::GetInstance()->RegisterRequestUrl(
            original_url_);
  }

  int64 delay = original_url_throttler_entry_->ReserveSendingTimeForNextRequest(
      GetBackoffReleaseTime());
  if (delay == 0) {
    StartURLRequest();
  } else {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        NewRunnableMethod(this, &Core::StartURLRequest),
        delay);
  }
}

void URLFetcher::Core::CancelURLRequest() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (request_.get()) {
    request_->Cancel();
    ReleaseRequest();
  }
  // Release the reference to the request context. There could be multiple
  // references to URLFetcher::Core at this point so it may take a while to
  // delete the object, but we cannot delay the destruction of the request
  // context.
  request_context_getter_ = NULL;
  was_cancelled_ = true;
}

void URLFetcher::Core::OnCompletedURLRequest(
    const net::URLRequestStatus& status) {
  DCHECK(delegate_loop_proxy_->BelongsToCurrentThread());

  // Checks the response from server.
  if (response_code_ >= 500 ||
      status.os_error() == net::ERR_TEMPORARILY_THROTTLED) {
    // When encountering a server error, we will send the request again
    // after backoff time.
    ++num_retries_;
    // Restarts the request if we still need to notify the delegate.
    if (delegate_) {
      fetcher_->backoff_delay_ = backoff_release_time_ - base::TimeTicks::Now();
      if (fetcher_->backoff_delay_ < base::TimeDelta())
        fetcher_->backoff_delay_ = base::TimeDelta();

      if (fetcher_->automatically_retry_on_5xx_ &&
          num_retries_ <= fetcher_->max_retries()) {
        io_message_loop_proxy_->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &Core::StartURLRequestWhenAppropriate));
      } else {
        delegate_->OnURLFetchComplete(fetcher_, url_, status, response_code_,
                                      cookies_, data_);
      }
    }
  } else {
    if (delegate_) {
      fetcher_->backoff_delay_ = base::TimeDelta();
      delegate_->OnURLFetchComplete(fetcher_, url_, status, response_code_,
                                    cookies_, data_);
    }
  }
}

void URLFetcher::Core::NotifyMalformedContent() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  if (url_throttler_entry_ != NULL)
    url_throttler_entry_->ReceivedContentWasMalformed();
}

void URLFetcher::Core::ReleaseRequest() {
  request_.reset();
  g_registry.Get().RemoveURLFetcherCore(this);
}

base::TimeTicks URLFetcher::Core::GetBackoffReleaseTime() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(original_url_throttler_entry_ != NULL);

  base::TimeTicks original_url_backoff =
      original_url_throttler_entry_->GetExponentialBackoffReleaseTime();
  base::TimeTicks destination_url_backoff;
  if (url_throttler_entry_ != NULL &&
      original_url_throttler_entry_ != url_throttler_entry_) {
    destination_url_backoff =
        url_throttler_entry_->GetExponentialBackoffReleaseTime();
  }

  return original_url_backoff > destination_url_backoff ?
      original_url_backoff : destination_url_backoff;
}

void URLFetcher::set_upload_data(const std::string& upload_content_type,
                                 const std::string& upload_content) {
  core_->upload_content_type_ = upload_content_type;
  core_->upload_content_ = upload_content;
}

const std::string& URLFetcher::upload_data() const {
  return core_->upload_content_;
}

void URLFetcher::set_referrer(const std::string& referrer) {
  core_->referrer_ = referrer;
}

void URLFetcher::set_load_flags(int load_flags) {
  core_->load_flags_ = load_flags;
}

int URLFetcher::load_flags() const {
  return core_->load_flags_;
}

void URLFetcher::set_extra_request_headers(
    const std::string& extra_request_headers) {
  core_->extra_request_headers_.Clear();
  core_->extra_request_headers_.AddHeadersFromString(extra_request_headers);
}

void URLFetcher::set_request_context(
    URLRequestContextGetter* request_context_getter) {
  core_->request_context_getter_ = request_context_getter;
}

void URLFetcher::set_automatically_retry_on_5xx(bool retry) {
  automatically_retry_on_5xx_ = retry;
}

net::HttpResponseHeaders* URLFetcher::response_headers() const {
  return core_->response_headers_;
}

void URLFetcher::Start() {
  core_->Start();
}

const GURL& URLFetcher::url() const {
  return core_->url_;
}

void URLFetcher::ReceivedContentWasMalformed() {
  core_->ReceivedContentWasMalformed();
}

// static
void URLFetcher::CancelAll() {
  Core::CancelAll();
}

URLFetcher::Delegate* URLFetcher::delegate() const {
  return core_->delegate();
}
