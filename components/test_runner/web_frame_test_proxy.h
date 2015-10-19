// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_
#define COMPONENTS_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_

#include "base/basictypes.h"
#include "components/test_runner/mock_screen_orientation_client.h"
#include "components/test_runner/web_test_delegate.h"
#include "components/test_runner/web_test_interfaces.h"
#include "components/test_runner/web_test_proxy.h"
#include "components/test_runner/web_test_runner.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace test_runner {

// Templetized wrapper around RenderFrameImpl objects, which implement
// the WebFrameClient interface.
template <class Base, typename P>
class WebFrameTestProxy : public Base {
 public:
  explicit WebFrameTestProxy(P p) : Base(p), base_proxy_(NULL) {}

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

  virtual void loadURLExternally(const blink::WebURLRequest& request,
                                 blink::WebNavigationPolicy policy,
                                 const blink::WebString& suggested_name,
                                 bool replaces_current_history_item) {
    base_proxy_->LoadURLExternally(request, policy, suggested_name,
                                   replaces_current_history_item);
    Base::loadURLExternally(request, policy, suggested_name,
                            replaces_current_history_item);
  }

  virtual void didStartProvisionalLoad(blink::WebLocalFrame* frame,
                                       double triggeringEventTime) {
    base_proxy_->DidStartProvisionalLoad(frame);
    Base::didStartProvisionalLoad(
        frame, triggeringEventTime);
  }

  virtual void didReceiveServerRedirectForProvisionalLoad(
      blink::WebLocalFrame* frame) {
    base_proxy_->DidReceiveServerRedirectForProvisionalLoad(frame);
    Base::didReceiveServerRedirectForProvisionalLoad(frame);
  }

  virtual void didFailProvisionalLoad(
      blink::WebLocalFrame* frame,
      const blink::WebURLError& error,
      blink::WebHistoryCommitType commit_type) {
    // If the test finished, don't notify the embedder of the failed load,
    // as we already destroyed the document loader.
    if (base_proxy_->DidFailProvisionalLoad(frame, error, commit_type))
      return;
    Base::didFailProvisionalLoad(frame, error, commit_type);
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

  virtual void didFinishDocumentLoad(blink::WebLocalFrame* frame, bool empty) {
    base_proxy_->DidFinishDocumentLoad(frame);
    Base::didFinishDocumentLoad(frame, empty);
  }

  virtual void didHandleOnloadEvents(blink::WebLocalFrame* frame) {
    base_proxy_->DidHandleOnloadEvents(frame);
    Base::didHandleOnloadEvents(frame);
  }

  virtual void didFailLoad(blink::WebLocalFrame* frame,
                           const blink::WebURLError& error,
                           blink::WebHistoryCommitType commit_type) {
    base_proxy_->DidFailLoad(frame, error, commit_type);
    Base::didFailLoad(frame, error, commit_type);
  }

  virtual void didFinishLoad(blink::WebLocalFrame* frame) {
    Base::didFinishLoad(frame);
    base_proxy_->DidFinishLoad(frame);
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
    base_proxy_->GetDelegate()->PrintMessage(std::string("ALERT: ") +
                                             message.utf8().data() + "\n");
  }

  virtual bool runModalConfirmDialog(const blink::WebString& message) {
    base_proxy_->GetDelegate()->PrintMessage(std::string("CONFIRM: ") +
                                             message.utf8().data() + "\n");
    return true;
  }

  virtual bool runModalPromptDialog(const blink::WebString& message,
                                    const blink::WebString& default_value,
                                    blink::WebString*) {
    base_proxy_->GetDelegate()->PrintMessage(
        std::string("PROMPT: ") + message.utf8().data() + ", default text: " +
        default_value.utf8().data() + "\n");
    return true;
  }

  virtual bool runModalBeforeUnloadDialog(bool is_reload,
                                          const blink::WebString& message) {
    base_proxy_->GetDelegate()->PrintMessage(
        std::string("CONFIRM NAVIGATION: ") + message.utf8().data() + "\n");
    return !base_proxy_->GetInterfaces()
                ->TestRunner()
                ->ShouldStayOnPageAfterHandlingBeforeUnload();
  }

  virtual void showContextMenu(
      const blink::WebContextMenuData& context_menu_data) {
    base_proxy_->ShowContextMenu(context_menu_data);
    Base::showContextMenu(context_menu_data);
  }

  virtual void didDetectXSS(const blink::WebURL& insecure_url,
                            bool did_block_entire_page) {
    // This is not implemented in RenderFrameImpl, so need to explicitly call
    // into the base proxy.
    base_proxy_->DidDetectXSS(insecure_url, did_block_entire_page);
    Base::didDetectXSS(insecure_url, did_block_entire_page);
  }

  virtual void didDispatchPingLoader(blink::WebLocalFrame* frame,
                                     const blink::WebURL& url) {
    // This is not implemented in RenderFrameImpl, so need to explicitly call
    // into the base proxy.
    base_proxy_->DidDispatchPingLoader(frame, url);
    Base::didDispatchPingLoader(frame, url);
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

  virtual void postAccessibilityEvent(const blink::WebAXObject& object,
                                      blink::WebAXEvent event) {
    base_proxy_->PostAccessibilityEvent(object, event);
    Base::postAccessibilityEvent(object, event);
  }

 private:
  WebTestProxyBase* base_proxy_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameTestProxy);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_
