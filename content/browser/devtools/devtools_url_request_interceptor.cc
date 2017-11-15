// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_url_request_interceptor.h"

#include "base/memory/ptr_util.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "content/browser/devtools/devtools_interceptor_controller.h"
#include "content/browser/devtools/devtools_url_interceptor_request_job.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"

namespace content {

InterceptedRequestInfo::InterceptedRequestInfo() = default;
InterceptedRequestInfo::~InterceptedRequestInfo() = default;

// static
bool DevToolsURLRequestInterceptor::IsNavigationRequest(
    ResourceType resource_type) {
  return resource_type == RESOURCE_TYPE_MAIN_FRAME ||
         resource_type == RESOURCE_TYPE_SUB_FRAME;
}

DevToolsURLRequestInterceptor::DevToolsURLRequestInterceptor(
    BrowserContext* browser_context)
    : next_id_(0), weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto target_registry = std::make_unique<DevToolsTargetRegistry>(
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO));
  target_resolver_ = target_registry->CreateResolver();
  // Controller lifetime is managed by the browser context.
  auto* controller = new DevToolsInterceptorController(
      weak_factory_.GetWeakPtr(), std::move(target_registry), browser_context);
  controller_ = controller->weak_factory_.GetWeakPtr();
}

DevToolsURLRequestInterceptor::~DevToolsURLRequestInterceptor() {
  // The BrowserContext owns us, so we don't need to unregister
  // DevToolsURLRequestInterceptorUserData explicitly.
}

DevToolsURLRequestInterceptor::Pattern::Pattern() = default;

DevToolsURLRequestInterceptor::Pattern::~Pattern() = default;

DevToolsURLRequestInterceptor::Pattern::Pattern(const Pattern& other) = default;

DevToolsURLRequestInterceptor::Pattern::Pattern(
    const std::string& url_pattern,
    base::flat_set<ResourceType> resource_types,
    InterceptionStage interception_stage)
    : url_pattern(url_pattern),
      resource_types(std::move(resource_types)),
      interception_stage(interception_stage) {}

const DevToolsTargetRegistry::TargetInfo*
DevToolsURLRequestInterceptor::TargetInfoForRequestInfo(
    const ResourceRequestInfo* request_info) const {
  int frame_node_id = request_info->GetFrameTreeNodeId();
  if (frame_node_id != -1)
    return target_resolver_->GetInfoByFrameTreeNodeId(frame_node_id);
  return target_resolver_->GetInfoByRenderFramePair(
      request_info->GetChildID(), request_info->GetRenderFrameID());
}

void DevToolsURLRequestInterceptor::ContinueInterceptedRequest(
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

net::URLRequestJob* DevToolsURLRequestInterceptor::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return const_cast<DevToolsURLRequestInterceptor*>(this)
      ->InnerMaybeInterceptRequest(request, network_delegate);
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

void DevToolsURLRequestInterceptor::GetResponseBody(
    std::string interception_id,
    std::unique_ptr<GetResponseBodyForInterceptionCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DevToolsURLInterceptorRequestJob* job = GetJob(interception_id);
  if (!job) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &GetResponseBodyForInterceptionCallback::sendFailure,
            std::move(callback),
            protocol::Response::InvalidParams("Invalid InterceptionId.")));
    return;
  }

  job->GetResponseBody(std::move(callback));
}

net::URLRequestJob* DevToolsURLRequestInterceptor::InnerMaybeInterceptRequest(
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

  InterceptionStage interception_stage = DONT_INTERCEPT;
  const std::string url =
      protocol::NetworkHandler::ClearUrlRef(request->url()).spec();
  for (const Pattern& pattern : intercepted_page.intercepted_patterns) {
    if (!pattern.resource_types.empty() &&
        pattern.resource_types.find(resource_request_info->GetResourceType()) ==
            pattern.resource_types.end()) {
      continue;
    }
    if (base::MatchPattern(url, pattern.url_pattern)) {
      if (pattern.interception_stage == REQUEST &&
          interception_stage == RESPONSE) {
        interception_stage = BOTH;
        break;
      } else if (pattern.interception_stage == RESPONSE &&
                 interception_stage == REQUEST) {
        interception_stage = BOTH;
        break;
      }
      interception_stage = pattern.interception_stage;
    }
  }

  if (interception_stage == DONT_INTERCEPT)
    return nullptr;

  bool is_redirect;
  std::string interception_id = GetIdForRequest(request, &is_redirect);

  if (IsNavigationRequest(resource_request_info->GetResourceType())) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&DevToolsInterceptorController::NavigationStarted,
                       controller_, interception_id,
                       resource_request_info->GetGlobalRequestID()));
  }

  DevToolsURLInterceptorRequestJob* job = new DevToolsURLInterceptorRequestJob(
      this, interception_id, request, network_delegate,
      target_info->devtools_token, target_info->devtools_target_id,
      intercepted_page.network_handler, is_redirect,
      resource_request_info->GetResourceType(), interception_stage);
  interception_id_to_job_map_[interception_id] = job;
  return job;
}

void DevToolsURLRequestInterceptor::StartInterceptingRequests(
    const base::UnguessableToken& target_id,
    std::unique_ptr<InterceptedPage> interceptedPage) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  target_id_to_intercepted_page_[target_id] = std::move(interceptedPage);
}

void DevToolsURLRequestInterceptor::StopInterceptingRequests(
    const base::UnguessableToken& target_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  target_id_to_intercepted_page_.erase(target_id);

  // Tell any jobs associated with associated agent host to stop intercepting.
  for (const auto pair : interception_id_to_job_map_) {
    if (pair.second->target_id() == target_id)
      pair.second->StopIntercepting();
  }
}

void DevToolsURLRequestInterceptor::RegisterSubRequest(
    const net::URLRequest* sub_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(sub_requests_.find(sub_request) == sub_requests_.end());
  sub_requests_.insert(sub_request);
}

void DevToolsURLRequestInterceptor::UnregisterSubRequest(
    const net::URLRequest* sub_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(sub_requests_.find(sub_request) != sub_requests_.end());
  sub_requests_.erase(sub_request);
}

void DevToolsURLRequestInterceptor::ExpectRequestAfterRedirect(
    const net::URLRequest* request,
    std::string id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  expected_redirects_[request] = id;
}

std::string DevToolsURLRequestInterceptor::GetIdForRequest(
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

DevToolsURLInterceptorRequestJob* DevToolsURLRequestInterceptor::GetJob(
    const std::string& interception_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const auto it = interception_id_to_job_map_.find(interception_id);
  if (it == interception_id_to_job_map_.end())
    return nullptr;
  return it->second;
}

void DevToolsURLRequestInterceptor::JobFinished(
    const std::string& interception_id,
    bool is_navigation) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  interception_id_to_job_map_.erase(interception_id);
  if (!is_navigation)
    return;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&DevToolsInterceptorController::NavigationFinished,
                     controller_, interception_id));
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

DevToolsURLRequestInterceptor::InterceptedPage::InterceptedPage(
    base::WeakPtr<protocol::NetworkHandler> network_handler,
    std::vector<Pattern> intercepted_patterns)
    : network_handler(network_handler),
      intercepted_patterns(std::move(intercepted_patterns)) {}

DevToolsURLRequestInterceptor::InterceptedPage::~InterceptedPage() = default;

}  // namespace content
