// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/throttling_url_loader.h"

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/common/browser_side_navigation_policy.h"

namespace content {

class ThrottlingURLLoader::ForwardingThrottleDelegate
    : public URLLoaderThrottle::Delegate {
 public:
  ForwardingThrottleDelegate(ThrottlingURLLoader* loader,
                             URLLoaderThrottle* throttle)
      : loader_(loader), throttle_(throttle) {}
  ~ForwardingThrottleDelegate() override = default;

  // URLLoaderThrottle::Delegate:
  void CancelWithError(int error_code) override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->CancelWithError(error_code);
  }

  void Resume() override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->StopDeferringForThrottle(throttle_);
  }

  void PauseReadingBodyFromNet() override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->PauseReadingBodyFromNet(throttle_);
  }

  void ResumeReadingBodyFromNet() override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->ResumeReadingBodyFromNet(throttle_);
  }

  void Detach() { loader_ = nullptr; }

 private:
  // This class helps ThrottlingURLLoader to keep track of whether it is being
  // called by its throttles.
  // If ThrottlingURLLoader is destoyed while any of the throttles is calling
  // into it, it delays destruction of the throttles. That way throttles don't
  // need to worry about any delegate calls may destory them synchronously.
  class ScopedDelegateCall {
   public:
    explicit ScopedDelegateCall(ForwardingThrottleDelegate* owner)
        : owner_(owner) {
      DCHECK(owner_->loader_);

      owner_->loader_->inside_delegate_calls_++;
    }

    ~ScopedDelegateCall() {
      // The loader may have been detached and destroyed.
      if (owner_->loader_)
        owner_->loader_->inside_delegate_calls_--;
    }

   private:
    ForwardingThrottleDelegate* const owner_;
    DISALLOW_COPY_AND_ASSIGN(ScopedDelegateCall);
  };

  ThrottlingURLLoader* loader_;
  URLLoaderThrottle* const throttle_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingThrottleDelegate);
};

ThrottlingURLLoader::StartInfo::StartInfo(
    mojom::URLLoaderFactory* in_url_loader_factory,
    int32_t in_routing_id,
    int32_t in_request_id,
    uint32_t in_options,
    StartLoaderCallback in_start_loader_callback,
    const ResourceRequest& in_url_request,
    scoped_refptr<base::SingleThreadTaskRunner> in_task_runner)
    : url_loader_factory(in_url_loader_factory),
      routing_id(in_routing_id),
      request_id(in_request_id),
      options(in_options),
      start_loader_callback(std::move(in_start_loader_callback)),
      url_request(in_url_request),
      task_runner(std::move(in_task_runner)) {}

ThrottlingURLLoader::StartInfo::~StartInfo() = default;

ThrottlingURLLoader::ResponseInfo::ResponseInfo(
    const ResourceResponseHead& in_response_head,
    const base::Optional<net::SSLInfo>& in_ssl_info,
    mojom::DownloadedTempFilePtr in_downloaded_file)
    : response_head(in_response_head),
      ssl_info(in_ssl_info),
      downloaded_file(std::move(in_downloaded_file)) {}

ThrottlingURLLoader::ResponseInfo::~ResponseInfo() = default;

ThrottlingURLLoader::RedirectInfo::RedirectInfo(
    const net::RedirectInfo& in_redirect_info,
    const ResourceResponseHead& in_response_head)
    : redirect_info(in_redirect_info), response_head(in_response_head) {}

ThrottlingURLLoader::RedirectInfo::~RedirectInfo() = default;

ThrottlingURLLoader::PriorityInfo::PriorityInfo(
    net::RequestPriority in_priority,
    int32_t in_intra_priority_value)
    : priority(in_priority), intra_priority_value(in_intra_priority_value) {}

ThrottlingURLLoader::PriorityInfo::~PriorityInfo() = default;

// static
std::unique_ptr<ThrottlingURLLoader> ThrottlingURLLoader::CreateLoaderAndStart(
    mojom::URLLoaderFactory* factory,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& url_request,
    mojom::URLLoaderClient* client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  std::unique_ptr<ThrottlingURLLoader> loader(new ThrottlingURLLoader(
      std::move(throttles), client, traffic_annotation));
  loader->Start(factory, routing_id, request_id, options, StartLoaderCallback(),
                url_request, std::move(task_runner));
  return loader;
}

// static
std::unique_ptr<ThrottlingURLLoader> ThrottlingURLLoader::CreateLoaderAndStart(
    StartLoaderCallback start_loader_callback,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    const ResourceRequest& url_request,
    mojom::URLLoaderClient* client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  std::unique_ptr<ThrottlingURLLoader> loader(new ThrottlingURLLoader(
      std::move(throttles), client, traffic_annotation));
  loader->Start(nullptr, 0, 0, mojom::kURLLoadOptionNone,
                std::move(start_loader_callback), url_request,
                std::move(task_runner));
  return loader;
}

ThrottlingURLLoader::~ThrottlingURLLoader() {
  if (inside_delegate_calls_ > 0) {
    // A throttle is calling into this object. In this case, delay destruction
    // of the throttles, so that throttles don't need to worry about any
    // delegate calls may destory them synchronously.
    for (auto& entry : throttles_)
      entry.delegate->Detach();

    auto throttles =
        std::make_unique<std::vector<ThrottleEntry>>(std::move(throttles_));
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                    std::move(throttles));
  }
}

void ThrottlingURLLoader::FollowRedirect() {
  if (url_loader_)
    url_loader_->FollowRedirect();
}

void ThrottlingURLLoader::SetPriority(net::RequestPriority priority,
                                      int32_t intra_priority_value) {
  if (!url_loader_) {
    if (!loader_cancelled_) {
      DCHECK_EQ(DEFERRED_START, deferred_stage_);
      priority_info_ =
          base::MakeUnique<PriorityInfo>(priority, intra_priority_value);
    }
    return;
  }

  url_loader_->SetPriority(priority, intra_priority_value);
}

void ThrottlingURLLoader::DisconnectClient() {
  client_binding_.Close();
  url_loader_ = nullptr;
  loader_cancelled_ = true;
}

ThrottlingURLLoader::ThrottlingURLLoader(
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    mojom::URLLoaderClient* client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : forwarding_client_(client),
      client_binding_(this),
      traffic_annotation_(traffic_annotation) {
  throttles_.reserve(throttles.size());
  for (auto& throttle : throttles)
    throttles_.emplace_back(this, std::move(throttle));
}

void ThrottlingURLLoader::Start(
    mojom::URLLoaderFactory* factory,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    StartLoaderCallback start_loader_callback,
    const ResourceRequest& url_request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_cancelled_);

  if (options & mojom::kURLLoadOptionSynchronous)
    is_synchronous_ = true;

  DCHECK(deferring_throttles_.empty());
  if (!throttles_.empty()) {
    bool deferred = false;
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      bool throttle_deferred = false;
      throttle->WillStartRequest(url_request, &throttle_deferred);
      if (!HandleThrottleResult(throttle, throttle_deferred, &deferred))
        return;
    }

    if (deferred) {
      deferred_stage_ = DEFERRED_START;
      start_info_ =
          base::MakeUnique<StartInfo>(factory, routing_id, request_id, options,
                                      std::move(start_loader_callback),
                                      url_request, std::move(task_runner));
      return;
    }
  }

  StartNow(factory, routing_id, request_id, options,
           std::move(start_loader_callback), url_request,
           std::move(task_runner));
}

void ThrottlingURLLoader::StartNow(
    mojom::URLLoaderFactory* factory,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    StartLoaderCallback start_loader_callback,
    const ResourceRequest& url_request,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  mojom::URLLoaderClientPtr client;
  client_binding_.Bind(mojo::MakeRequest(&client), std::move(task_runner));
  client_binding_.set_connection_error_handler(base::Bind(
      &ThrottlingURLLoader::OnClientConnectionError, base::Unretained(this)));

  if (factory) {
    DCHECK(!start_loader_callback);

    factory->CreateLoaderAndStart(
        mojo::MakeRequest(&url_loader_), routing_id, request_id, options,
        url_request, std::move(client),
        net::MutableNetworkTrafficAnnotationTag(traffic_annotation_));
  } else {
    std::move(start_loader_callback)
        .Run(mojo::MakeRequest(&url_loader_), std::move(client));
  }

  if (!pausing_reading_body_from_net_throttles_.empty())
    url_loader_->PauseReadingBodyFromNet();

  if (priority_info_) {
    auto priority_info = std::move(priority_info_);
    url_loader_->SetPriority(priority_info->priority,
                             priority_info->intra_priority_value);
  }
}

bool ThrottlingURLLoader::HandleThrottleResult(URLLoaderThrottle* throttle,
                                               bool throttle_deferred,
                                               bool* should_defer) {
  DCHECK(!deferring_throttles_.count(throttle));
  if (loader_cancelled_)
    return false;
  *should_defer |= throttle_deferred;
  if (throttle_deferred)
    deferring_throttles_.insert(throttle);
  return true;
}

void ThrottlingURLLoader::StopDeferringForThrottle(
    URLLoaderThrottle* throttle) {
  if (deferring_throttles_.find(throttle) == deferring_throttles_.end())
    return;

  deferring_throttles_.erase(throttle);
  if (deferring_throttles_.empty() && !loader_cancelled_)
    Resume();
}

void ThrottlingURLLoader::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    const base::Optional<net::SSLInfo>& ssl_info,
    mojom::DownloadedTempFilePtr downloaded_file) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_cancelled_);
  DCHECK(deferring_throttles_.empty());

  if (!throttles_.empty()) {
    bool deferred = false;
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      bool throttle_deferred = false;
      throttle->WillProcessResponse(response_head, &throttle_deferred);
      if (!HandleThrottleResult(throttle, throttle_deferred, &deferred))
        return;
    }

    if (deferred) {
      deferred_stage_ = DEFERRED_RESPONSE;
      response_info_ = base::MakeUnique<ResponseInfo>(
          response_head, ssl_info, std::move(downloaded_file));
      client_binding_.PauseIncomingMethodCallProcessing();
      return;
    }
  }

  forwarding_client_->OnReceiveResponse(response_head, ssl_info,
                                        std::move(downloaded_file));
}

void ThrottlingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_cancelled_);
  DCHECK(deferring_throttles_.empty());

  if (!throttles_.empty()) {
    bool deferred = false;
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      bool throttle_deferred = false;
      throttle->WillRedirectRequest(redirect_info, &throttle_deferred);
      if (!HandleThrottleResult(throttle, throttle_deferred, &deferred))
        return;
    }

    if (deferred) {
      deferred_stage_ = DEFERRED_REDIRECT;
      redirect_info_ =
          base::MakeUnique<RedirectInfo>(redirect_info, response_head);
      client_binding_.PauseIncomingMethodCallProcessing();
      return;
    }
  }

  forwarding_client_->OnReceiveRedirect(redirect_info, response_head);
}

void ThrottlingURLLoader::OnDataDownloaded(int64_t data_len,
                                           int64_t encoded_data_len) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_cancelled_);

  forwarding_client_->OnDataDownloaded(data_len, encoded_data_len);
}

void ThrottlingURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_cancelled_);

  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(ack_callback));
}

void ThrottlingURLLoader::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_cancelled_);

  forwarding_client_->OnReceiveCachedMetadata(data);
}

void ThrottlingURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_cancelled_);

  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void ThrottlingURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_cancelled_);

  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void ThrottlingURLLoader::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_cancelled_);

  // This is the last expected message. Pipe closure before this is an error
  // (see OnClientConnectionError). After this it is expected and should be
  // ignored.
  DisconnectClient();
  forwarding_client_->OnComplete(status);
}

void ThrottlingURLLoader::OnClientConnectionError() {
  // TODO(reillyg): Temporary workaround for crbug.com/756751 where without
  // browser-side navigation this error on async loads will confuse the loading
  // of cross-origin iframes.
  if (is_synchronous_ || content::IsBrowserSideNavigationEnabled())
    CancelWithError(net::ERR_FAILED);
}

void ThrottlingURLLoader::CancelWithError(int error_code) {
  if (loader_cancelled_)
    return;

  ResourceRequestCompletionStatus request_complete_data;
  request_complete_data.error_code = error_code;
  request_complete_data.completion_time = base::TimeTicks::Now();

  deferred_stage_ = DEFERRED_NONE;
  DisconnectClient();
  forwarding_client_->OnComplete(request_complete_data);
}

void ThrottlingURLLoader::Resume() {
  if (loader_cancelled_ || deferred_stage_ == DEFERRED_NONE)
    return;

  auto prev_deferred_stage = deferred_stage_;
  deferred_stage_ = DEFERRED_NONE;
  switch (prev_deferred_stage) {
    case DEFERRED_START: {
      StartNow(start_info_->url_loader_factory, start_info_->routing_id,
               start_info_->request_id, start_info_->options,
               std::move(start_info_->start_loader_callback),
               start_info_->url_request, std::move(start_info_->task_runner));
      break;
    }
    case DEFERRED_REDIRECT: {
      client_binding_.ResumeIncomingMethodCallProcessing();
      forwarding_client_->OnReceiveRedirect(redirect_info_->redirect_info,
                                            redirect_info_->response_head);
      break;
    }
    case DEFERRED_RESPONSE: {
      client_binding_.ResumeIncomingMethodCallProcessing();
      forwarding_client_->OnReceiveResponse(
          response_info_->response_head, response_info_->ssl_info,
          std::move(response_info_->downloaded_file));
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void ThrottlingURLLoader::PauseReadingBodyFromNet(URLLoaderThrottle* throttle) {
  if (pausing_reading_body_from_net_throttles_.empty() && url_loader_)
    url_loader_->PauseReadingBodyFromNet();

  pausing_reading_body_from_net_throttles_.insert(throttle);
}

void ThrottlingURLLoader::ResumeReadingBodyFromNet(
    URLLoaderThrottle* throttle) {
  auto iter = pausing_reading_body_from_net_throttles_.find(throttle);
  if (iter == pausing_reading_body_from_net_throttles_.end())
    return;

  pausing_reading_body_from_net_throttles_.erase(iter);
  if (pausing_reading_body_from_net_throttles_.empty() && url_loader_)
    url_loader_->ResumeReadingBodyFromNet();
}

ThrottlingURLLoader::ThrottleEntry::ThrottleEntry(
    ThrottlingURLLoader* loader,
    std::unique_ptr<URLLoaderThrottle> the_throttle)
    : delegate(
          base::MakeUnique<ForwardingThrottleDelegate>(loader,
                                                       the_throttle.get())),
      throttle(std::move(the_throttle)) {
  throttle->set_delegate(delegate.get());
}

ThrottlingURLLoader::ThrottleEntry::ThrottleEntry(ThrottleEntry&& other) =
    default;

ThrottlingURLLoader::ThrottleEntry::~ThrottleEntry() = default;

ThrottlingURLLoader::ThrottleEntry& ThrottlingURLLoader::ThrottleEntry::
operator=(ThrottleEntry&& other) = default;

}  // namespace content
