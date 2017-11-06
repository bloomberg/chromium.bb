// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_interceptor_controller.h"

#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"

namespace content {

namespace {
const char kDevToolsInterceptorController[] = "DevToolsInterceptorController";
}

// static
void DevToolsInterceptorController::SetUserData(
    BrowserContext* browser_context,
    base::WeakPtr<DevToolsURLRequestInterceptor> interceptor,
    std::unique_ptr<DevToolsTargetRegistry> target_registry) {
  std::unique_ptr<DevToolsInterceptorController> controller(
      new DevToolsInterceptorController(interceptor,
                                        std::move(target_registry)));
  browser_context->SetUserData(kDevToolsInterceptorController,
                               std::move(controller));
}

void DevToolsInterceptorController::StartInterceptingRequests(
    const FrameTreeNode* target_frame,
    base::WeakPtr<protocol::NetworkHandler> network_handler,
    std::vector<Pattern> intercepted_patterns) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const base::UnguessableToken& target_id =
      target_frame->devtools_frame_token();
  target_handles_[target_id] = target_registry_->RegisterWebContents(
      WebContentsImpl::FromFrameTreeNode(target_frame));

  std::unique_ptr<DevToolsURLRequestInterceptor::InterceptedPage>
      intercepted_page(new DevToolsURLRequestInterceptor::InterceptedPage(
          std::move(network_handler), intercepted_patterns));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&DevToolsURLRequestInterceptor::StartInterceptingRequests,
                     interceptor_, target_id, std::move(intercepted_page)));
}

void DevToolsInterceptorController::StopInterceptingRequests(
    const FrameTreeNode* target_frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const base::UnguessableToken& target_id =
      target_frame->devtools_frame_token();
  if (!target_handles_.erase(target_id))
    return;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&DevToolsURLRequestInterceptor::StopInterceptingRequests,
                     interceptor_, target_id));
}

void DevToolsInterceptorController::ContinueInterceptedRequest(
    std::string interception_id,
    std::unique_ptr<Modifications> modifications,
    std::unique_ptr<ContinueInterceptedRequestCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&DevToolsURLRequestInterceptor::ContinueInterceptedRequest,
                     interceptor_, interception_id,
                     base::Passed(std::move(modifications)),
                     base::Passed(std::move(callback))));
}

// static
DevToolsInterceptorController*
DevToolsInterceptorController::FromBrowserContext(BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return static_cast<DevToolsInterceptorController*>(
      context->GetUserData(kDevToolsInterceptorController));
}

DevToolsInterceptorController::DevToolsInterceptorController(
    base::WeakPtr<DevToolsURLRequestInterceptor> interceptor,
    std::unique_ptr<DevToolsTargetRegistry> target_registry)
    : interceptor_(interceptor), target_registry_(std::move(target_registry)) {}

DevToolsInterceptorController::~DevToolsInterceptorController() = default;

}  // namespace content
