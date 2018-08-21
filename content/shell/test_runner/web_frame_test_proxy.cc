// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_frame_test_proxy.h"

#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_view_test_proxy.h"

namespace test_runner {

WebFrameTestProxy::~WebFrameTestProxy() = default;

void WebFrameTestProxy::Initialize(
    WebTestInterfaces* interfaces,
    content::RenderViewImpl* render_view_for_frame) {
  // The RenderViewImpl will also be a test proxy type.
  auto* view_proxy_for_frame =
      static_cast<WebViewTestProxy*>(render_view_for_frame);

  test_client_ =
      interfaces->CreateWebFrameTestClient(view_proxy_for_frame, this);
}

// WebLocalFrameClient implementation.
blink::WebPlugin* WebFrameTestProxy::CreatePlugin(
    const blink::WebPluginParams& params) {
  blink::WebPlugin* plugin = test_client_->CreatePlugin(params);
  if (plugin)
    return plugin;
  return RenderFrameImpl::CreatePlugin(params);
}

void WebFrameTestProxy::DidAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
  test_client_->DidAddMessageToConsole(message, source_name, source_line,
                                       stack_trace);
  RenderFrameImpl::DidAddMessageToConsole(message, source_name, source_line,
                                          stack_trace);
}

void WebFrameTestProxy::DownloadURL(
    const blink::WebURLRequest& request,
    blink::WebLocalFrameClient::CrossOriginRedirects
        cross_origin_redirect_behavior,
    mojo::ScopedMessagePipeHandle blob_url_token) {
  test_client_->DownloadURL(request, cross_origin_redirect_behavior,
                            mojo::ScopedMessagePipeHandle());
  RenderFrameImpl::DownloadURL(request, cross_origin_redirect_behavior,
                               std::move(blob_url_token));
}

void WebFrameTestProxy::DidStartProvisionalLoad(
    blink::WebDocumentLoader* document_loader,
    blink::WebURLRequest& request,
    const base::TimeTicks& input_start) {
  test_client_->DidStartProvisionalLoad(document_loader, request, input_start);
  RenderFrameImpl::DidStartProvisionalLoad(document_loader, request,
                                           input_start);
}

void WebFrameTestProxy::DidFailProvisionalLoad(
    const blink::WebURLError& error,
    blink::WebHistoryCommitType commit_type) {
  test_client_->DidFailProvisionalLoad(error, commit_type);
  // If the test finished, don't notify the embedder of the failed load,
  // as we already destroyed the document loader.
  if (!web_frame()->GetProvisionalDocumentLoader())
    return;
  RenderFrameImpl::DidFailProvisionalLoad(error, commit_type);
}

void WebFrameTestProxy::DidCommitProvisionalLoad(
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type,
    blink::WebGlobalObjectReusePolicy global_object_reuse_policy) {
  test_client_->DidCommitProvisionalLoad(item, commit_type,
                                         global_object_reuse_policy);
  RenderFrameImpl::DidCommitProvisionalLoad(item, commit_type,
                                            global_object_reuse_policy);
}

void WebFrameTestProxy::DidFinishSameDocumentNavigation(
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type,
    bool content_initiated) {
  test_client_->DidFinishSameDocumentNavigation(item, commit_type,
                                                content_initiated);
  RenderFrameImpl::DidFinishSameDocumentNavigation(item, commit_type,
                                                   content_initiated);
}

void WebFrameTestProxy::DidReceiveTitle(const blink::WebString& title,
                                        blink::WebTextDirection direction) {
  test_client_->DidReceiveTitle(title, direction);
  RenderFrameImpl::DidReceiveTitle(title, direction);
}

void WebFrameTestProxy::DidChangeIcon(blink::WebIconURL::Type icon_type) {
  test_client_->DidChangeIcon(icon_type);
  RenderFrameImpl::DidChangeIcon(icon_type);
}

void WebFrameTestProxy::DidFinishDocumentLoad() {
  test_client_->DidFinishDocumentLoad();
  RenderFrameImpl::DidFinishDocumentLoad();
}

void WebFrameTestProxy::DidHandleOnloadEvents() {
  test_client_->DidHandleOnloadEvents();
  RenderFrameImpl::DidHandleOnloadEvents();
}

void WebFrameTestProxy::DidFailLoad(const blink::WebURLError& error,
                                    blink::WebHistoryCommitType commit_type) {
  test_client_->DidFailLoad(error, commit_type);
  RenderFrameImpl::DidFailLoad(error, commit_type);
}

void WebFrameTestProxy::DidFinishLoad() {
  RenderFrameImpl::DidFinishLoad();
  test_client_->DidFinishLoad();
}

void WebFrameTestProxy::DidStopLoading() {
  RenderFrameImpl::DidStopLoading();
  test_client_->DidStopLoading();
}

void WebFrameTestProxy::DidChangeSelection(bool is_selection_empty) {
  test_client_->DidChangeSelection(is_selection_empty);
  RenderFrameImpl::DidChangeSelection(is_selection_empty);
}

void WebFrameTestProxy::DidChangeContents() {
  test_client_->DidChangeContents();
  RenderFrameImpl::DidChangeContents();
}

blink::WebEffectiveConnectionType
WebFrameTestProxy::GetEffectiveConnectionType() {
  if (test_client_->GetEffectiveConnectionType() !=
      blink::WebEffectiveConnectionType::kTypeUnknown) {
    return test_client_->GetEffectiveConnectionType();
  }
  return RenderFrameImpl::GetEffectiveConnectionType();
}

void WebFrameTestProxy::RunModalAlertDialog(const blink::WebString& message) {
  test_client_->RunModalAlertDialog(message);
}

bool WebFrameTestProxy::RunModalConfirmDialog(const blink::WebString& message) {
  return test_client_->RunModalConfirmDialog(message);
}

bool WebFrameTestProxy::RunModalPromptDialog(
    const blink::WebString& message,
    const blink::WebString& default_value,
    blink::WebString* actual_value) {
  return test_client_->RunModalPromptDialog(message, default_value,
                                            actual_value);
}

bool WebFrameTestProxy::RunModalBeforeUnloadDialog(bool is_reload) {
  return test_client_->RunModalBeforeUnloadDialog(is_reload);
}

void WebFrameTestProxy::ShowContextMenu(
    const blink::WebContextMenuData& context_menu_data) {
  test_client_->ShowContextMenu(context_menu_data);
  RenderFrameImpl::ShowContextMenu(context_menu_data);
}

void WebFrameTestProxy::DidDetectXSS(const blink::WebURL& insecure_url,
                                     bool did_block_entire_page) {
  // This is not implemented in RenderFrameImpl, so need to explicitly call
  // into the base proxy.
  test_client_->DidDetectXSS(insecure_url, did_block_entire_page);
  RenderFrameImpl::DidDetectXSS(insecure_url, did_block_entire_page);
}

void WebFrameTestProxy::DidDispatchPingLoader(const blink::WebURL& url) {
  // This is not implemented in RenderFrameImpl, so need to explicitly call
  // into the base proxy.
  test_client_->DidDispatchPingLoader(url);
  RenderFrameImpl::DidDispatchPingLoader(url);
}

void WebFrameTestProxy::WillSendRequest(blink::WebURLRequest& request) {
  RenderFrameImpl::WillSendRequest(request);
  test_client_->WillSendRequest(request);
}

void WebFrameTestProxy::DidReceiveResponse(
    const blink::WebURLResponse& response) {
  test_client_->DidReceiveResponse(response);
  RenderFrameImpl::DidReceiveResponse(response);
}

blink::WebNavigationPolicy WebFrameTestProxy::DecidePolicyForNavigation(
    const blink::WebLocalFrameClient::NavigationPolicyInfo& info) {
  blink::WebNavigationPolicy policy =
      test_client_->DecidePolicyForNavigation(info);
  if (policy == blink::kWebNavigationPolicyIgnore)
    return policy;

  return RenderFrameImpl::DecidePolicyForNavigation(info);
}

void WebFrameTestProxy::PostAccessibilityEvent(const blink::WebAXObject& object,
                                               blink::WebAXEvent event) {
  test_client_->PostAccessibilityEvent(object, event);
  // Guard against the case where |this| was deleted as a result of an
  // accessibility listener detaching a frame. If that occurs, the
  // WebAXObject will be detached.
  if (object.IsDetached())
    return;  // |this| is invalid.
  RenderFrameImpl::PostAccessibilityEvent(object, event);
}

void WebFrameTestProxy::CheckIfAudioSinkExistsAndIsAuthorized(
    const blink::WebString& sink_id,
    blink::WebSetSinkIdCallbacks* web_callbacks) {
  test_client_->CheckIfAudioSinkExistsAndIsAuthorized(sink_id, web_callbacks);
}

void WebFrameTestProxy::DidClearWindowObject() {
  test_client_->DidClearWindowObject();
  RenderFrameImpl::DidClearWindowObject();
}

bool WebFrameTestProxy::RunFileChooser(
    const blink::WebFileChooserParams& params,
    blink::WebFileChooserCompletion* completion) {
  return test_client_->RunFileChooser(params, completion);
}

}  // namespace test_runner
