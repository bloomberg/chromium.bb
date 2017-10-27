// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_url_request_interceptor.h"

#include "base/memory/ptr_util.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_url_interceptor_request_job.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {
const char kDevToolsURLRequestInterceptorKeyName[] =
    "DevToolsURLRequestInterceptor";

class DevToolsURLRequestInterceptorUserData
    : public base::SupportsUserData::Data {
 public:
  explicit DevToolsURLRequestInterceptorUserData(
      DevToolsURLRequestInterceptor* devtools_url_request_interceptor)
      : devtools_url_request_interceptor_(devtools_url_request_interceptor) {}

  DevToolsURLRequestInterceptor* devtools_url_request_interceptor() const {
    return devtools_url_request_interceptor_;
  }

 private:
  DevToolsURLRequestInterceptor* devtools_url_request_interceptor_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsURLRequestInterceptorUserData);
};

}  // namespace

DevToolsURLRequestInterceptor::DevToolsURLRequestInterceptor(
    BrowserContext* browser_context)
    : browser_context_(browser_context), state_(new State()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_context_->SetUserData(
      kDevToolsURLRequestInterceptorKeyName,
      std::make_unique<DevToolsURLRequestInterceptorUserData>(this));
}

DevToolsURLRequestInterceptor::~DevToolsURLRequestInterceptor() {
  // The BrowserContext owns us, so we don't need to unregister
  // DevToolsURLRequestInterceptorUserData explicitly.
}

net::URLRequestJob* DevToolsURLRequestInterceptor::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return state_->MaybeCreateDevToolsURLInterceptorRequestJob(request,
                                                             network_delegate);
}

void DevToolsURLRequestInterceptor::StartInterceptingRequests(
    const FrameTreeNode* target_frame,
    base::WeakPtr<protocol::NetworkHandler> network_handler,
    std::vector<Pattern> intercepted_patterns) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const base::UnguessableToken& target_id =
      target_frame->devtools_frame_token();
  target_handles_[target_id] = state_->target_registry_->RegisterWebContents(
      WebContentsImpl::FromFrameTreeNode(target_frame));

  std::unique_ptr<State::InterceptedPage> intercepted_page(
      new State::InterceptedPage(std::move(network_handler),
                                 intercepted_patterns));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &DevToolsURLRequestInterceptor::State::StartInterceptingRequestsOnIO,
          state_, target_id, std::move(intercepted_page)));
}

void DevToolsURLRequestInterceptor::StopInterceptingRequests(
    const FrameTreeNode* target_frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const base::UnguessableToken& target_id =
      target_frame->devtools_frame_token();
  if (!target_handles_.erase(target_id))
    return;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &DevToolsURLRequestInterceptor::State::StopInterceptingRequestsOnIO,
          state_, target_id));
}

void DevToolsURLRequestInterceptor::ContinueInterceptedRequest(
    std::string interception_id,
    std::unique_ptr<Modifications> modifications,
    std::unique_ptr<ContinueInterceptedRequestCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &DevToolsURLRequestInterceptor::State::ContinueInterceptedRequestOnIO,
          state_, interception_id, base::Passed(std::move(modifications)),
          base::Passed(std::move(callback))));
}

net::URLRequestJob* DevToolsURLRequestInterceptor::MaybeInterceptRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const GURL& location) const {
  return nullptr;
}

net::URLRequestJob* DevToolsURLRequestInterceptor::MaybeInterceptResponse(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return nullptr;
}

DevToolsURLRequestInterceptor::Pattern::Pattern() = default;

DevToolsURLRequestInterceptor::Pattern::~Pattern() = default;

DevToolsURLRequestInterceptor::Pattern::Pattern(const Pattern& other) = default;

DevToolsURLRequestInterceptor::Pattern::Pattern(
    const std::string& url_pattern,
    base::flat_set<ResourceType> resource_types)
    : url_pattern(url_pattern), resource_types(std::move(resource_types)) {}

DevToolsURLRequestInterceptor::State::State()
    : target_registry_(new DevToolsTargetRegistry(
          content::BrowserThread::GetTaskRunnerForThread(
              content::BrowserThread::IO))),
      next_id_(0) {}

DevToolsURLRequestInterceptor::State::~State() {}

const DevToolsTargetRegistry::TargetInfo*
DevToolsURLRequestInterceptor::State::TargetInfoForRequestInfo(
    const ResourceRequestInfo* request_info) {
  int frame_node_id = request_info->GetFrameTreeNodeId();
  if (frame_node_id != -1)
    return target_registry_->GetInfoByFrameTreeNodeId(frame_node_id);
  return target_registry_->GetInfoByRenderFramePair(
      request_info->GetChildID(), request_info->GetRenderFrameID());
}

void DevToolsURLRequestInterceptor::State::ContinueInterceptedRequestOnIO(
    std::string interception_id,
    std::unique_ptr<Modifications> modifications,
    std::unique_ptr<ContinueInterceptedRequestCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DevToolsURLInterceptorRequestJob* job = GetJob(interception_id);
  if (!job) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &ContinueInterceptedRequestCallback::sendFailure,
            base::Passed(std::move(callback)),
            protocol::Response::InvalidParams("Invalid InterceptionId.")));
    return;
  }

  job->ContinueInterceptedRequest(std::move(modifications),
                                  std::move(callback));
}

DevToolsURLInterceptorRequestJob* DevToolsURLRequestInterceptor::State::
    MaybeCreateDevToolsURLInterceptorRequestJob(
        net::URLRequest* request,
        net::NetworkDelegate* network_delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Bail out if we're not intercepting anything.
  if (target_id_to_intercepted_page_.empty())
    return nullptr;
  // Don't try to intercept blob resources.
  if (request->url().SchemeIsBlob())
    return nullptr;
  const ResourceRequestInfo* resource_request_info =
      ResourceRequestInfo::ForRequest(request);
  if (!resource_request_info)
    return nullptr;
  const DevToolsTargetRegistry::TargetInfo* target_info =
      TargetInfoForRequestInfo(resource_request_info);
  if (!target_info)
    return nullptr;
  auto it =
      target_id_to_intercepted_page_.find(target_info->devtools_target_id);
  if (it == target_id_to_intercepted_page_.end())
    return nullptr;
  const InterceptedPage& intercepted_page = *it->second;

  // We don't want to intercept our own sub requests.
  if (sub_requests_.find(request) != sub_requests_.end())
    return nullptr;

  bool matchFound = false;
  const std::string url =
      protocol::NetworkHandler::ClearUrlRef(request->url()).spec();
  for (const Pattern& pattern : intercepted_page.intercepted_patterns) {
    if (!pattern.resource_types.empty() &&
        pattern.resource_types.find(resource_request_info->GetResourceType()) ==
            pattern.resource_types.end()) {
      continue;
    }
    if (base::MatchPattern(url, pattern.url_pattern)) {
      matchFound = true;
      break;
    }
  }
  if (!matchFound)
    return nullptr;

  bool is_redirect;
  std::string interception_id = GetIdForRequestOnIO(request, &is_redirect);
  DevToolsURLInterceptorRequestJob* job = new DevToolsURLInterceptorRequestJob(
      this, interception_id, request, network_delegate,
      target_info->devtools_token, target_info->devtools_target_id,
      intercepted_page.network_handler, is_redirect,
      resource_request_info->GetResourceType());
  interception_id_to_job_map_[interception_id] = job;
  return job;
}

void DevToolsURLRequestInterceptor::State::StartInterceptingRequestsOnIO(
    const base::UnguessableToken& target_id,
    std::unique_ptr<InterceptedPage> interceptedPage) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  target_id_to_intercepted_page_[target_id] = std::move(interceptedPage);
}

void DevToolsURLRequestInterceptor::State::StopInterceptingRequestsOnIO(
    const base::UnguessableToken& target_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  target_id_to_intercepted_page_.erase(target_id);

  // Tell any jobs associated with associated agent host to stop intercepting.
  for (const auto pair : interception_id_to_job_map_) {
    if (pair.second->target_id() == target_id)
      pair.second->StopIntercepting();
  }
}

void DevToolsURLRequestInterceptor::State::RegisterSubRequest(
    const net::URLRequest* sub_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(sub_requests_.find(sub_request) == sub_requests_.end());
  sub_requests_.insert(sub_request);
}

void DevToolsURLRequestInterceptor::State::UnregisterSubRequest(
    const net::URLRequest* sub_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(sub_requests_.find(sub_request) != sub_requests_.end());
  sub_requests_.erase(sub_request);
}

void DevToolsURLRequestInterceptor::State::ExpectRequestAfterRedirect(
    const net::URLRequest* request,
    std::string id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  expected_redirects_[request] = id;
}

std::string DevToolsURLRequestInterceptor::State::GetIdForRequestOnIO(
    const net::URLRequest* request,
    bool* is_redirect) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto find_it = expected_redirects_.find(request);
  if (find_it == expected_redirects_.end()) {
    *is_redirect = false;
    return base::StringPrintf("id-%zu", ++next_id_);
  }
  *is_redirect = true;
  std::string id = find_it->second;
  expected_redirects_.erase(find_it);
  return id;
}

DevToolsURLInterceptorRequestJob* DevToolsURLRequestInterceptor::State::GetJob(
    const std::string& interception_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const auto it = interception_id_to_job_map_.find(interception_id);
  if (it == interception_id_to_job_map_.end())
    return nullptr;
  return it->second;
}

void DevToolsURLRequestInterceptor::State::JobFinished(
    const std::string& interception_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  interception_id_to_job_map_.erase(interception_id);
}

// static
DevToolsURLRequestInterceptor*
DevToolsURLRequestInterceptor::FromBrowserContext(BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* interceptor_user_data =
      static_cast<DevToolsURLRequestInterceptorUserData*>(
          context->GetUserData(kDevToolsURLRequestInterceptorKeyName));
  if (!interceptor_user_data)
    return nullptr;
  return interceptor_user_data->devtools_url_request_interceptor();
}

DevToolsURLRequestInterceptor::Modifications::Modifications(
    base::Optional<net::Error> error_reason,
    base::Optional<std::string> raw_response,
    protocol::Maybe<std::string> modified_url,
    protocol::Maybe<std::string> modified_method,
    protocol::Maybe<std::string> modified_post_data,
    protocol::Maybe<protocol::Network::Headers> modified_headers,
    protocol::Maybe<protocol::Network::AuthChallengeResponse>
        auth_challenge_response,
    bool mark_as_canceled)
    : error_reason(std::move(error_reason)),
      raw_response(std::move(raw_response)),
      modified_url(std::move(modified_url)),
      modified_method(std::move(modified_method)),
      modified_post_data(std::move(modified_post_data)),
      modified_headers(std::move(modified_headers)),
      auth_challenge_response(std::move(auth_challenge_response)),
      mark_as_canceled(mark_as_canceled) {}

DevToolsURLRequestInterceptor::Modifications::~Modifications() {}

DevToolsURLRequestInterceptor::State::InterceptedPage::InterceptedPage(
    base::WeakPtr<protocol::NetworkHandler> network_handler,
    std::vector<Pattern> intercepted_patterns)
    : network_handler(network_handler),
      intercepted_patterns(std::move(intercepted_patterns)) {}

DevToolsURLRequestInterceptor::State::InterceptedPage::~InterceptedPage() =
    default;

}  // namespace content
