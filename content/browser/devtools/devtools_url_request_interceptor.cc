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
      base::MakeUnique<DevToolsURLRequestInterceptorUserData>(this));
}

DevToolsURLRequestInterceptor::~DevToolsURLRequestInterceptor() {
  // The BrowserContext owns us, so we don't need to unregister
  // DevToolsURLRequestInterceptorUserData explicitly.
}

net::URLRequestJob* DevToolsURLRequestInterceptor::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return state()->MaybeCreateDevToolsURLInterceptorRequestJob(request,
                                                              network_delegate);
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

DevToolsURLRequestInterceptor::State::State() : next_id_(0) {}

DevToolsURLRequestInterceptor::State::~State() {}

DevToolsURLRequestInterceptor::State::RenderFrameHostInfo::RenderFrameHostInfo(
    RenderFrameHost* host)
    : routing_id(host->GetRoutingID()),
      frame_tree_node_id(host->GetFrameTreeNodeId()),
      process_id(host->GetProcess()->GetID()) {}

void DevToolsURLRequestInterceptor::State::ContinueInterceptedRequest(
    std::string interception_id,
    std::unique_ptr<Modifications> modifications,
    std::unique_ptr<ContinueInterceptedRequestCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &DevToolsURLRequestInterceptor::State::ContinueInterceptedRequestOnIO,
          this, interception_id, base::Passed(std::move(modifications)),
          base::Passed(std::move(callback))));
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
  if (intercepted_render_frames_.empty()) {
    DCHECK(intercepted_frame_tree_nodes_.empty());
    return nullptr;
  }

  // Don't try to intercept blob resources.
  if (request->url().SchemeIsBlob())
    return nullptr;

  const ResourceRequestInfo* resource_request_info =
      ResourceRequestInfo::ForRequest(request);
  if (!resource_request_info)
    return nullptr;
  int child_id = resource_request_info->GetChildID();
  int frame_tree_node_id = resource_request_info->GetFrameTreeNodeId();
  WebContents* web_contents = nullptr;
  if (frame_tree_node_id == -1) {
    // |frame_tree_node_id| is not set for renderer side requests, fall back to
    // the RenderFrameID.
    int render_frame_id = resource_request_info->GetRenderFrameID();
    const auto find_it = intercepted_render_frames_.find(
        std::make_pair(render_frame_id, child_id));
    if (find_it == intercepted_render_frames_.end())
      return nullptr;
    web_contents = find_it->second;
  } else {
    // |frame_tree_node_id| is set for browser side navigations, so use that
    // because the RenderFrameID isn't known (neither is the ChildID).
    const auto find_it = intercepted_frame_tree_nodes_.find(frame_tree_node_id);
    if (find_it == intercepted_frame_tree_nodes_.end())
      return nullptr;
    web_contents = find_it->second;
  }

  DCHECK(intercepted_page_for_web_contents_.count(web_contents));

  const InterceptedPage& intercepted_page =
      *intercepted_page_for_web_contents_.find(web_contents)->second;

  // We don't want to intercept our own sub requests.
  if (sub_requests_.find(request) != sub_requests_.end())
    return nullptr;

  // If we are only intercepting certain resource types then bail out if the
  // request is for the wrong type.
  if (!intercepted_page.intercepted_resource_types.empty() &&
      intercepted_page.intercepted_resource_types.find(
          resource_request_info->GetResourceType()) ==
          intercepted_page.intercepted_resource_types.end()) {
    return nullptr;
  }

  bool matchFound = false;
  for (const std::string& pattern : intercepted_page.intercepted_url_patterns) {
    if (base::MatchPattern(request->url().spec(), pattern)) {
      matchFound = true;
      break;
    }
  }
  if (!matchFound)
    return nullptr;

  bool is_redirect;
  std::string interception_id = GetIdForRequestOnIO(request, &is_redirect);
  DevToolsURLInterceptorRequestJob* job = new DevToolsURLInterceptorRequestJob(
      this, interception_id, request, network_delegate, web_contents,
      intercepted_page.network_handler, is_redirect,
      resource_request_info->GetResourceType());
  interception_id_to_job_map_[interception_id] = job;
  return job;
}

class DevToolsURLRequestInterceptor::State::InterceptedWebContentsObserver
    : public WebContentsObserver {
 public:
  InterceptedWebContentsObserver(
      WebContents* web_contents,
      scoped_refptr<DevToolsURLRequestInterceptor::State> state,
      base::WeakPtr<protocol::NetworkHandler> network_handler)
      : WebContentsObserver(web_contents), state_(state) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::Optional<DevToolsURLRequestInterceptor::State::RenderFrameHostInfo>
        old_host_info;
    DevToolsURLRequestInterceptor::State::RenderFrameHostInfo new_host_info(
        new_host);
    if (old_host) {
      old_host_info.emplace(
          DevToolsURLRequestInterceptor::State::RenderFrameHostInfo(old_host));
    }

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(
            &DevToolsURLRequestInterceptor::State::RenderFrameHostChangedOnIO,
            state_, old_host_info, new_host_info, web_contents()));
  }

  void FrameDeleted(RenderFrameHost* render_frame_host) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(
            &DevToolsURLRequestInterceptor::State::
                StopInterceptingRequestsForHostInfoOnIO,
            state_,
            DevToolsURLRequestInterceptor::State::RenderFrameHostInfo(
                render_frame_host)));
  }

 private:
  scoped_refptr<DevToolsURLRequestInterceptor::State> state_;
};

void DevToolsURLRequestInterceptor::State::RenderFrameHostChangedOnIO(
    base::Optional<RenderFrameHostInfo> old_host_info,
    RenderFrameHostInfo new_host_info,
    WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (old_host_info)
    StopInterceptingRequestsForHostInfoOnIO(old_host_info.value());
  StartInterceptingRequestsForHostInfoOnIOInternal(new_host_info, web_contents);
}

void DevToolsURLRequestInterceptor::State::
    StartInterceptingRequestsForHostInfoOnIOInternal(
        RenderFrameHostInfo host_info,
        WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(intercepted_page_for_web_contents_.count(web_contents));

  // We can assume that if there are frames already in the map they are in the
  // same web_contents as before.
  intercepted_render_frames_[std::make_pair(
      host_info.routing_id, host_info.process_id)] = web_contents;
  intercepted_frame_tree_nodes_[host_info.frame_tree_node_id] = web_contents;
}

void DevToolsURLRequestInterceptor::State::StartInterceptingRequestsOnIO(
    std::vector<RenderFrameHostInfo> host_info_list,
    WebContents* web_contents,
    std::unique_ptr<InterceptedPage> interceptedPage) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  intercepted_page_for_web_contents_[web_contents] = std::move(interceptedPage);
  for (const auto host_info : host_info_list)
    StartInterceptingRequestsForHostInfoOnIOInternal(host_info, web_contents);
}

void DevToolsURLRequestInterceptor::State::
    StopInterceptingRequestsForHostInfoOnIO(RenderFrameHostInfo host_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Since Devtools has not unregistered to the web_contents, we do not remove
  // it from the map.
  intercepted_render_frames_.erase(
      std::make_pair(host_info.routing_id, host_info.process_id));
  intercepted_frame_tree_nodes_.erase(host_info.frame_tree_node_id);
}

void DevToolsURLRequestInterceptor::State::StartInterceptingRequests(
    WebContents* web_contents,
    base::WeakPtr<protocol::NetworkHandler> network_handler,
    std::vector<std::string> intercepted_url_patterns,
    base::flat_set<ResourceType> intercepted_resource_types) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<RenderFrameHostInfo> host_info_list;
  for (RenderFrameHost* render_frame_host : web_contents->GetAllFrames())
    host_info_list.push_back(RenderFrameHostInfo(render_frame_host));

  std::unique_ptr<InterceptedPage> intercepted_page(
      new InterceptedPage(network_handler, std::move(intercepted_url_patterns),
                          std::move(intercepted_resource_types)));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &DevToolsURLRequestInterceptor::State::StartInterceptingRequestsOnIO,
          this, std::move(host_info_list), web_contents,
          std::move(intercepted_page)));

  // Listen for future updates.
  observers_.emplace(web_contents,
                     base::MakeUnique<InterceptedWebContentsObserver>(
                         web_contents, this, network_handler));
}

void DevToolsURLRequestInterceptor::State::StopInterceptingRequests(
    WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.erase(web_contents);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &DevToolsURLRequestInterceptor::State::StopInterceptingRequestsOnIO,
          this, web_contents));
}

void DevToolsURLRequestInterceptor::State::StopInterceptingRequestsOnIO(
    WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Remove any intercepted render frames associated with |web_contents|.
  base::flat_map<std::pair<int, int>, WebContents*>
      remaining_intercepted_render_frames;
  for (const auto& pair : intercepted_render_frames_) {
    DCHECK(intercepted_page_for_web_contents_.count(pair.second));
    if (pair.second == web_contents)
      continue;
    remaining_intercepted_render_frames.insert(pair);
  }
  std::swap(remaining_intercepted_render_frames, intercepted_render_frames_);

  // Remove any intercepted frame tree nodes associated with |web_contents|.
  base::flat_map<FrameTreeNodeId, WebContents*>
      remaining_intercepted_frame_tree_nodes;
  for (const auto& pair : intercepted_frame_tree_nodes_) {
    DCHECK(intercepted_page_for_web_contents_.count(pair.second));
    if (pair.second == web_contents)
      continue;
    remaining_intercepted_frame_tree_nodes.insert(pair);
  }
  std::swap(remaining_intercepted_frame_tree_nodes,
            intercepted_frame_tree_nodes_);

  intercepted_page_for_web_contents_.erase(web_contents);

  // Tell any jobs associated with |web_contents| to stop intercepting.
  for (const auto pair : interception_id_to_job_map_) {
    if (pair.second->web_contents() == web_contents)
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
    std::vector<std::string> intercepted_url_patterns,
    base::flat_set<ResourceType> intercepted_resource_types)
    : network_handler(network_handler),
      intercepted_url_patterns(std::move(intercepted_url_patterns)),
      intercepted_resource_types(std::move(intercepted_resource_types)) {}

DevToolsURLRequestInterceptor::State::InterceptedPage::~InterceptedPage() =
    default;

}  // namespace content
