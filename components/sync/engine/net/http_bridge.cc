// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/net/http_bridge.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bit_cast.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "components/sync/base/cancelation_signal.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/zlib/google/compression_utils.h"

namespace syncer {

namespace {

// It's possible for an http request to be silently stalled. We set a time
// limit for all http requests, beyond which the request is cancelled and
// treated as a transient failure.
const int kMaxHttpRequestTimeSeconds = 60 * 5;  // 5 minutes.

// Helper method for logging timeouts via UMA.
void LogTimeout(bool timed_out) {
  UMA_HISTOGRAM_BOOLEAN("Sync.URLFetchTimedOut", timed_out);
}

base::LazyInstance<scoped_refptr<base::SequencedTaskRunner>>::Leaky
    g_io_capable_task_runner_for_tests = LAZY_INSTANCE_INITIALIZER;

}  // namespace

HttpBridgeFactory::HttpBridgeFactory(
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    const NetworkTimeUpdateCallback& network_time_update_callback,
    CancelationSignal* cancelation_signal)
    : network_time_update_callback_(network_time_update_callback),
      cancelation_signal_(cancelation_signal) {
  // Some tests pass null'ed out url_loader_factory_info instances.
  if (url_loader_factory_info) {
    url_loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(url_loader_factory_info));
  }
  // This registration is happening on the Sync thread, while signalling occurs
  // on the UI thread. We must handle the possibility signalling has already
  // occurred.
  if (cancelation_signal_->TryRegisterHandler(this)) {
    registered_for_cancelation_ = true;
  } else {
    OnSignalReceived();
  }
}

HttpBridgeFactory::~HttpBridgeFactory() {
  if (registered_for_cancelation_) {
    cancelation_signal_->UnregisterHandler(this);
  }
}

void HttpBridgeFactory::Init(
    const std::string& user_agent,
    const BindToTrackerCallback& bind_to_tracker_callback) {
  user_agent_ = user_agent;
  bind_to_tracker_callback_ = bind_to_tracker_callback;
}

HttpPostProviderInterface* HttpBridgeFactory::Create() {
  DCHECK(url_loader_factory_);

  scoped_refptr<HttpBridge> http =
      new HttpBridge(user_agent_, url_loader_factory_->Clone(),
                     network_time_update_callback_, bind_to_tracker_callback_);
  http->AddRef();
  return http.get();
}

void HttpBridgeFactory::Destroy(HttpPostProviderInterface* http) {
  static_cast<HttpBridge*>(http)->Release();
}

void HttpBridgeFactory::OnSignalReceived() {
  // TODO(tonikitoo): Remove this method.
  //
  // Prior to the URLLoader conversion the sync code was holding on to a
  // scoped_refptr of a URLRequestContextGetter it was changing the lifetime
  // of net objects. With URLLoader, it's only holding on to mojo pipes
  // that issue doesn't exist.
  NOTIMPLEMENTED();
}

HttpBridge::URLFetchState::URLFetchState()
    : aborted(false),
      request_completed(false),
      request_succeeded(false),
      http_status_code(-1),
      net_error_code(-1) {}
HttpBridge::URLFetchState::~URLFetchState() {}

HttpBridge::HttpBridge(
    const std::string& user_agent,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    const NetworkTimeUpdateCallback& network_time_update_callback,
    const BindToTrackerCallback& bind_to_tracker_callback)
    : user_agent_(user_agent),
      http_post_completed_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED),
      url_loader_factory_info_(std::move(url_loader_factory_info)),
      network_task_runner_(
          g_io_capable_task_runner_for_tests.Get()
              ? g_io_capable_task_runner_for_tests.Get()
              : base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()})),
      network_time_update_callback_(network_time_update_callback),
      bind_to_tracker_callback_(bind_to_tracker_callback) {}

HttpBridge::~HttpBridge() {}

void HttpBridge::SetExtraRequestHeaders(const char* headers) {
  DCHECK(extra_headers_.empty())
      << "HttpBridge::SetExtraRequestHeaders called twice.";
  extra_headers_.assign(headers);
}

void HttpBridge::SetURL(const char* url, int port) {
#if DCHECK_IS_ON()
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    base::AutoLock lock(fetch_state_lock_);
    DCHECK(!fetch_state_.request_completed);
  }
  DCHECK(url_for_request_.is_empty())
      << "HttpBridge::SetURL called more than once?!";
#endif
  GURL temp(url);
  GURL::Replacements replacements;
  std::string port_str = base::NumberToString(port);
  replacements.SetPort(port_str.c_str(), url::Component(0, port_str.length()));
  url_for_request_ = temp.ReplaceComponents(replacements);
}

void HttpBridge::SetPostPayload(const char* content_type,
                                int content_length,
                                const char* content) {
#if DCHECK_IS_ON()
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    base::AutoLock lock(fetch_state_lock_);
    DCHECK(!fetch_state_.request_completed);
  }
  DCHECK(content_type_.empty()) << "Bridge payload already set.";
  DCHECK_GE(content_length, 0) << "Content length < 0";
#endif
  content_type_ = content_type;
  if (!content || (content_length == 0)) {
    DCHECK_EQ(content_length, 0);
    request_content_ = " ";  // TODO(timsteele): URLFetcher requires non-empty
                             // content for POSTs whereas CURL does not, for now
                             // we hack this to support the sync backend.
  } else {
    request_content_.assign(content, content_length);
  }
}

bool HttpBridge::MakeSynchronousPost(int* net_error_code,
                                     int* http_status_code) {
#if DCHECK_IS_ON()
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    base::AutoLock lock(fetch_state_lock_);
    DCHECK(!fetch_state_.request_completed);
  }
  DCHECK(url_for_request_.is_valid()) << "Invalid URL for request";
  DCHECK(!content_type_.empty()) << "Payload not set";
#endif

  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&HttpBridge::CallMakeAsynchronousPost, this))) {
    // This usually happens when we're in a unit test.
    LOG(WARNING) << "Could not post CallMakeAsynchronousPost task";
    return false;
  }

  // Block until network request completes or is aborted. See
  // OnURLFetchComplete and Abort.
  http_post_completed_.Wait();

  base::AutoLock lock(fetch_state_lock_);
  DCHECK(fetch_state_.request_completed || fetch_state_.aborted);
  *net_error_code = fetch_state_.net_error_code;
  *http_status_code = fetch_state_.http_status_code;
  return fetch_state_.request_succeeded;
}

void HttpBridge::MakeAsynchronousPost() {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock lock(fetch_state_lock_);
  DCHECK(!fetch_state_.request_completed);
  if (fetch_state_.aborted)
    return;

  // Start the timer on the network thread (the same thread progress is made
  // on, and on which the url fetcher lives).
  DCHECK(!fetch_state_.http_request_timeout_timer);
  fetch_state_.http_request_timeout_timer =
      std::make_unique<base::OneShotTimer>();
  fetch_state_.http_request_timeout_timer->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kMaxHttpRequestTimeSeconds),
      base::BindOnce(&HttpBridge::OnURLLoadTimedOut, this));

  // Some tests inject |url_loader_factory_| created to operated on the
  // IO-capable thread currently running.
  DCHECK((!url_loader_factory_ && url_loader_factory_info_) ||
         (url_loader_factory_ && !url_loader_factory_info_));
  if (!url_loader_factory_) {
    DCHECK(url_loader_factory_info_);
    url_loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(url_loader_factory_info_));
  }

  fetch_state_.start_time = base::Time::Now();
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("sync_http_bridge", R"(
        semantics {
          sender: "Chrome Sync"
          description:
            "Chrome Sync synchronizes profile data between Chromium clients "
            "and Google for a given user account."
          trigger:
            "User makes a change to syncable profile data after enabling sync "
            "on the device."
          data:
            "The device and user identifiers, along with any profile data that "
            "is changing."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can disable Chrome Sync by going into the profile settings "
            "and choosing to Sign Out."
          chrome_policy {
            SyncDisabled {
              policy_options {mode: MANDATORY}
              SyncDisabled: true
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_for_request_;
  resource_request->method = "POST";
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES;

  if (!extra_headers_.empty())
    resource_request->headers.AddHeadersFromString(extra_headers_);

  resource_request->headers.SetHeader("Content-Encoding", "gzip");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                      user_agent_);

  fetch_state_.url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  network::SimpleURLLoader* url_loader = fetch_state_.url_loader.get();

  // TODO(https://crbug.com/808498): Re-add data use measurement once
  // SimpleURLLoader supports it.
  //
  // This calls |BindFetcherToDataTracker| in
  // components/sync/driver/glue/sync_engine_backend.cc which used to
  // data_use_measurement::DataUseUserData::AttachToFetcher.
  // if (!bind_to_tracker_callback_.is_null())
  //  bind_to_tracker_callback_.Run(fetch_state_.url_poster);

  std::string request_to_send;
  compression::GzipCompress(request_content_, &request_to_send);
  url_loader->AttachStringForUpload(request_to_send, content_type_);

  // Sync relies on HTTP errors being detectable (and distinct from
  // net/connection errors).
  url_loader->SetAllowHttpErrorResults(true);

  url_loader->SetOnUploadProgressCallback(base::BindRepeating(
      &HttpBridge::OnURLLoadUploadProgress, base::Unretained(this)));
  url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&HttpBridge::OnURLLoadComplete, base::Unretained(this)));
}

int HttpBridge::GetResponseContentLength() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(fetch_state_lock_);
  DCHECK(fetch_state_.request_completed);
  return fetch_state_.response_content.size();
}

const char* HttpBridge::GetResponseContent() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(fetch_state_lock_);
  DCHECK(fetch_state_.request_completed);
  return fetch_state_.response_content.data();
}

const std::string HttpBridge::GetResponseHeaderValue(
    const std::string& name) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(fetch_state_lock_);
  DCHECK(fetch_state_.request_completed);

  std::string value;
  fetch_state_.response_headers->EnumerateHeader(nullptr, name, &value);
  return value;
}

void HttpBridge::Abort() {
  base::AutoLock lock(fetch_state_lock_);

  // Release |url_loader_factory_info_| as soon as possible so that
  // no URLLoaderFactory instances proceed on the network task runner.
  url_loader_factory_info_.reset();

  DCHECK(!fetch_state_.aborted);
  if (fetch_state_.aborted || fetch_state_.request_completed)
    return;

  fetch_state_.aborted = true;
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&HttpBridge::DestroyURLLoaderOnIOThread, this,
                         std::move(fetch_state_.url_loader),
                         std::move(fetch_state_.http_request_timeout_timer)))) {
    // Madness ensues.
    NOTREACHED() << "Could not post task to delete URLLoader";
  }

  fetch_state_.net_error_code = net::ERR_ABORTED;
  http_post_completed_.Signal();
}

void HttpBridge::DestroyURLLoaderOnIOThread(
    std::unique_ptr<network::SimpleURLLoader> loader,
    std::unique_ptr<base::OneShotTimer> loader_timer) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  // Both |loader_timer| and |loader| go out of scope.
  url_loader_factory_ = nullptr;
}

void HttpBridge::OnURLLoadComplete(std::unique_ptr<std::string> response_body) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock lock(fetch_state_lock_);

  network::SimpleURLLoader* url_loader = fetch_state_.url_loader.get();
  // If the fetch completes in the window between Abort() and
  // DestroyURLLoaderOnIOThread() this will still run. Abort() has already
  // reported the result, so no extra work is needed.
  if (fetch_state_.aborted)
    return;

  int http_status_code = -1;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    http_status_code = url_loader->ResponseInfo()->headers->response_code();
    fetch_state_.response_headers = url_loader->ResponseInfo()->headers;
  }

  OnURLLoadCompleteInternal(http_status_code, url_loader->NetError(),
                            url_loader->GetFinalURL(),
                            std::move(response_body));
}

void HttpBridge::OnURLLoadCompleteInternal(
    int http_status_code,
    int net_error_code,
    const GURL& final_url,
    std::unique_ptr<std::string> response_body) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  // Stop the request timer now that the request completed.
  if (fetch_state_.http_request_timeout_timer)
    fetch_state_.http_request_timeout_timer.reset();

  // TODO(crbug.com/844968): Relax this if-check to become a DCHECK?
  if (fetch_state_.aborted)
    return;

  fetch_state_.end_time = base::Time::Now();
  fetch_state_.request_completed = true;
  fetch_state_.request_succeeded =
      net_error_code == net::OK && http_status_code != -1;
  fetch_state_.http_status_code = http_status_code;
  fetch_state_.net_error_code = net_error_code;

  if (fetch_state_.request_succeeded)
    LogTimeout(false);
  base::UmaHistogramSparse(
      "Sync.URLFetchResponse",
      fetch_state_.request_succeeded
          ? fetch_state_.http_status_code
          : net::URLRequestStatus::FromError(fetch_state_.net_error_code)
                .ToNetError());
  UMA_HISTOGRAM_LONG_TIMES("Sync.URLFetchTime",
                           fetch_state_.end_time - fetch_state_.start_time);

  // Use a real (non-debug) log to facilitate troubleshooting in the wild.
  VLOG(2) << "HttpBridge::OnURLFetchComplete for: " << final_url.spec();
  VLOG(1) << "HttpBridge received response code: "
          << fetch_state_.http_status_code;

  if (response_body)
    fetch_state_.response_content = std::move(*response_body);

  UpdateNetworkTime();

  fetch_state_.url_loader.reset();
  url_loader_factory_ = nullptr;

  // Wake the blocked syncer thread in MakeSynchronousPost.
  // WARNING: DONT DO ANYTHING AFTER THIS CALL! |this| may be deleted!
  http_post_completed_.Signal();
}

void HttpBridge::OnURLLoadUploadProgress(uint64_t position, uint64_t total) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  // Reset the delay when forward progress is made.
  base::AutoLock lock(fetch_state_lock_);
  if (fetch_state_.http_request_timeout_timer)
    fetch_state_.http_request_timeout_timer->Reset();
}

void HttpBridge::OnURLLoadTimedOut() {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock lock(fetch_state_lock_);
  if (!fetch_state_.url_loader)
    return;

  LogTimeout(true);
  DVLOG(1) << "Sync url fetch timed out. Canceling.";

  fetch_state_.end_time = base::Time::Now();
  fetch_state_.request_completed = true;
  fetch_state_.request_succeeded = false;
  fetch_state_.http_status_code = -1;
  fetch_state_.net_error_code = net::ERR_TIMED_OUT;

  // This method is called by the timer, not the url fetcher implementation,
  // so it's safe to delete the fetcher here.
  fetch_state_.url_loader.reset();
  url_loader_factory_ = nullptr;

  // Timer is smart enough to handle being deleted as part of the invoked task.
  fetch_state_.http_request_timeout_timer.reset();

  // Wake the blocked syncer thread in MakeSynchronousPost.
  // WARNING: DONT DO ANYTHING AFTER THIS CALL! |this| may be deleted!
  http_post_completed_.Signal();
}

void HttpBridge::SetIOCapableTaskRunnerForTest(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  g_io_capable_task_runner_for_tests.Get() = task_runner;
}

void HttpBridge::UpdateNetworkTime() {
  std::string sane_time_str;
  if (!fetch_state_.request_succeeded || fetch_state_.start_time.is_null() ||
      fetch_state_.end_time < fetch_state_.start_time ||
      !fetch_state_.response_headers ||
      !fetch_state_.response_headers->EnumerateHeader(
          nullptr, "Sane-Time-Millis", &sane_time_str)) {
    return;
  }

  int64_t sane_time_ms = 0;
  if (base::StringToInt64(sane_time_str, &sane_time_ms)) {
    network_time_update_callback_.Run(
        base::Time::FromJsTime(sane_time_ms),
        base::TimeDelta::FromMilliseconds(1),
        fetch_state_.end_time - fetch_state_.start_time);
  }
}

}  // namespace syncer
