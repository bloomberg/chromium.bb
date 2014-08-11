// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "content/shell/renderer/test_runner/mock_screen_orientation_client.h"
#include "content/shell/renderer/test_runner/test_interfaces.h"
#include "content/shell/renderer/test_runner/test_runner.h"
#include "content/shell/renderer/test_runner/web_test_proxy.h"
#include "content/test/test_media_stream_renderer_factory.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

// Templetized wrapper around RenderFrameImpl objects, which implement
// the WebFrameClient interface.
template <class Base, typename P, typename R>
class WebFrameTestProxy : public Base {
 public:
  WebFrameTestProxy(P p, R r) : Base(p, r), base_proxy_(NULL) {}

  virtual ~WebFrameTestProxy() {}

  void set_base_proxy(WebTestProxyBase* proxy) { base_proxy_ = proxy; }

  // WebFrameClient implementation.
  virtual blink::WebPlugin* createPlugin(blink::WebLocalFrame* frame,
                                         const blink::WebPluginParams& params) {
    blink::WebPlugin* plugin = base_proxy_->CreatePlugin(frame, params);
    if (plugin)
      return plugin;
    return Base::createPlugin(frame, params);
  }

  virtual blink::WebScreenOrientationClient* webScreenOrientationClient() {
    return base_proxy_->GetScreenOrientationClientMock();
  }

  virtual void didAddMessageToConsole(const blink::WebConsoleMessage& message,
                                      const blink::WebString& source_name,
                                      unsigned source_line,
                                      const blink::WebString& stack_trace) {
    base_proxy_->DidAddMessageToConsole(message, source_name, source_line);
    Base::didAddMessageToConsole(
        message, source_name, source_line, stack_trace);
  }

  virtual bool canCreatePluginWithoutRenderer(
      const blink::WebString& mime_type) {
    using blink::WebString;

    const CR_DEFINE_STATIC_LOCAL(
        WebString, suffix, ("-can-create-without-renderer"));
    return mime_type.utf8().find(suffix.utf8()) != std::string::npos;
  }

  virtual void loadURLExternally(blink::WebLocalFrame* frame,
                                 const blink::WebURLRequest& request,
                                 blink::WebNavigationPolicy policy,
                                 const blink::WebString& suggested_name) {
    base_proxy_->LoadURLExternally(frame, request, policy, suggested_name);
    Base::loadURLExternally(frame, request, policy, suggested_name);
  }

  virtual void didStartProvisionalLoad(blink::WebLocalFrame* frame,
                                       bool isTransitionNavigation) {
    base_proxy_->DidStartProvisionalLoad(frame);
    Base::didStartProvisionalLoad(frame, isTransitionNavigation);
  }

  virtual void didReceiveServerRedirectForProvisionalLoad(
      blink::WebLocalFrame* frame) {
    base_proxy_->DidReceiveServerRedirectForProvisionalLoad(frame);
    Base::didReceiveServerRedirectForProvisionalLoad(frame);
  }

  virtual void didFailProvisionalLoad(blink::WebLocalFrame* frame,
                                      const blink::WebURLError& error) {
    // If the test finished, don't notify the embedder of the failed load,
    // as we already destroyed the document loader.
    if (base_proxy_->DidFailProvisionalLoad(frame, error))
      return;
    Base::didFailProvisionalLoad(frame, error);
  }

  virtual void didCommitProvisionalLoad(
      blink::WebLocalFrame* frame,
      const blink::WebHistoryItem& item,
      blink::WebHistoryCommitType commit_type) {
    base_proxy_->DidCommitProvisionalLoad(frame, item, commit_type);
    Base::didCommitProvisionalLoad(frame, item, commit_type);
  }

  virtual void didReceiveTitle(blink::WebLocalFrame* frame,
                               const blink::WebString& title,
                               blink::WebTextDirection direction) {
    base_proxy_->DidReceiveTitle(frame, title, direction);
    Base::didReceiveTitle(frame, title, direction);
  }

  virtual void didChangeIcon(blink::WebLocalFrame* frame,
                             blink::WebIconURL::Type icon_type) {
    base_proxy_->DidChangeIcon(frame, icon_type);
    Base::didChangeIcon(frame, icon_type);
  }

  virtual void didFinishDocumentLoad(blink::WebLocalFrame* frame) {
    base_proxy_->DidFinishDocumentLoad(frame);
    Base::didFinishDocumentLoad(frame);
  }

  virtual void didHandleOnloadEvents(blink::WebLocalFrame* frame) {
    base_proxy_->DidHandleOnloadEvents(frame);
    Base::didHandleOnloadEvents(frame);
  }

  virtual void didFailLoad(blink::WebLocalFrame* frame,
                           const blink::WebURLError& error) {
    base_proxy_->DidFailLoad(frame, error);
    Base::didFailLoad(frame, error);
  }

  virtual void didFinishLoad(blink::WebLocalFrame* frame) {
    Base::didFinishLoad(frame);
    base_proxy_->DidFinishLoad(frame);
  }

  virtual blink::WebNotificationPresenter* notificationPresenter() {
    return base_proxy_->GetNotificationPresenter();
  }

  virtual void didChangeSelection(bool is_selection_empty) {
    base_proxy_->DidChangeSelection(is_selection_empty);
    Base::didChangeSelection(is_selection_empty);
  }

  virtual blink::WebColorChooser* createColorChooser(
      blink::WebColorChooserClient* client,
      const blink::WebColor& initial_color,
      const blink::WebVector<blink::WebColorSuggestion>& suggestions) {
    return base_proxy_->CreateColorChooser(client, initial_color, suggestions);
  }

  virtual void runModalAlertDialog(const blink::WebString& message) {
    base_proxy_->delegate_->printMessage(std::string("ALERT: ") +
                                         message.utf8().data() + "\n");
  }

  virtual bool runModalConfirmDialog(const blink::WebString& message) {
    base_proxy_->delegate_->printMessage(std::string("CONFIRM: ") +
                                         message.utf8().data() + "\n");
    return true;
  }

  virtual bool runModalPromptDialog(const blink::WebString& message,
                                    const blink::WebString& default_value,
                                    blink::WebString*) {
    base_proxy_->delegate_->printMessage(
        std::string("PROMPT: ") + message.utf8().data() + ", default text: " +
        default_value.utf8().data() + "\n");
    return true;
  }

  virtual bool runModalBeforeUnloadDialog(bool is_reload,
                                          const blink::WebString& message) {
    base_proxy_->delegate_->printMessage(std::string("CONFIRM NAVIGATION: ") +
                                         message.utf8().data() + "\n");
    return !base_proxy_->test_interfaces_->GetTestRunner()
                ->shouldStayOnPageAfterHandlingBeforeUnload();
  }

  virtual void showContextMenu(
      const blink::WebContextMenuData& context_menu_data) {
    base_proxy_->ShowContextMenu(Base::GetWebFrame()->toWebLocalFrame(),
                                 context_menu_data);
    Base::showContextMenu(context_menu_data);
  }

  virtual void didDetectXSS(blink::WebLocalFrame* frame,
                            const blink::WebURL& insecure_url,
                            bool did_block_entire_page) {
    // This is not implemented in RenderFrameImpl, so need to explicitly call
    // into the base proxy.
    base_proxy_->DidDetectXSS(frame, insecure_url, did_block_entire_page);
    Base::didDetectXSS(frame, insecure_url, did_block_entire_page);
  }

  virtual void didDispatchPingLoader(blink::WebLocalFrame* frame,
                                     const blink::WebURL& url) {
    // This is not implemented in RenderFrameImpl, so need to explicitly call
    // into the base proxy.
    base_proxy_->DidDispatchPingLoader(frame, url);
    Base::didDispatchPingLoader(frame, url);
  }

  virtual void willRequestResource(blink::WebLocalFrame* frame,
                                   const blink::WebCachedURLRequest& request) {
    // This is not implemented in RenderFrameImpl, so need to explicitly call
    // into the base proxy.
    base_proxy_->WillRequestResource(frame, request);
    Base::willRequestResource(frame, request);
  }

  virtual void didCreateDataSource(blink::WebLocalFrame* frame,
                                   blink::WebDataSource* ds) {
    Base::didCreateDataSource(frame, ds);
  }

  virtual void willSendRequest(blink::WebLocalFrame* frame,
                               unsigned identifier,
                               blink::WebURLRequest& request,
                               const blink::WebURLResponse& redirect_response) {
    Base::willSendRequest(frame, identifier, request, redirect_response);
    base_proxy_->WillSendRequest(frame, identifier, request, redirect_response);
  }

  virtual void didReceiveResponse(blink::WebLocalFrame* frame,
                                  unsigned identifier,
                                  const blink::WebURLResponse& response) {
    base_proxy_->DidReceiveResponse(frame, identifier, response);
    Base::didReceiveResponse(frame, identifier, response);
  }

  virtual void didChangeResourcePriority(
      blink::WebLocalFrame* frame,
      unsigned identifier,
      const blink::WebURLRequest::Priority& priority,
      int intra_priority_value) {
    // This is not implemented in RenderFrameImpl, so need to explicitly call
    // into the base proxy.
    base_proxy_->DidChangeResourcePriority(
        frame, identifier, priority, intra_priority_value);
    Base::didChangeResourcePriority(
        frame, identifier, priority, intra_priority_value);
  }

  virtual void didFinishResourceLoad(blink::WebLocalFrame* frame,
                                     unsigned identifier) {
    base_proxy_->DidFinishResourceLoad(frame, identifier);
    Base::didFinishResourceLoad(frame, identifier);
  }

  virtual blink::WebNavigationPolicy decidePolicyForNavigation(
      const blink::WebFrameClient::NavigationPolicyInfo& info) {
    blink::WebNavigationPolicy policy = base_proxy_->DecidePolicyForNavigation(
        info);
    if (policy == blink::WebNavigationPolicyIgnore)
      return policy;

    return Base::decidePolicyForNavigation(info);
  }

  virtual void willStartUsingPeerConnectionHandler(
      blink::WebLocalFrame* frame,
      blink::WebRTCPeerConnectionHandler* handler) {
    // RenderFrameImpl::willStartUsingPeerConnectionHandler can not be mocked.
    // See http://crbug/363285.
  }

  virtual blink::WebUserMediaClient* userMediaClient() {
    return base_proxy_->GetUserMediaClient();
  }

  virtual blink::WebMIDIClient* webMIDIClient() {
    return base_proxy_->GetWebMIDIClient();
  }

  virtual bool willCheckAndDispatchMessageEvent(
      blink::WebLocalFrame* source_frame,
      blink::WebFrame* target_frame,
      blink::WebSecurityOrigin target,
      blink::WebDOMMessageEvent event) {
    if (base_proxy_->WillCheckAndDispatchMessageEvent(
            source_frame, target_frame, target, event))
      return true;
    return Base::willCheckAndDispatchMessageEvent(
        source_frame, target_frame, target, event);
  }

  virtual void didStopLoading() {
    base_proxy_->DidStopLoading();
    Base::didStopLoading();
  }

 private:
#if defined(ENABLE_WEBRTC)
  virtual scoped_ptr<MediaStreamRendererFactory> CreateRendererFactory()
      OVERRIDE {
    return scoped_ptr<MediaStreamRendererFactory>(
        new TestMediaStreamRendererFactory());
  }
#endif

  WebTestProxyBase* base_proxy_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameTestProxy);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_
