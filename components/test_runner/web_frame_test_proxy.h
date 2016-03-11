// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_
#define COMPONENTS_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_

#include "base/macros.h"
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
  blink::WebPlugin* createPlugin(
      blink::WebLocalFrame* frame,
      const blink::WebPluginParams& params) override {
    blink::WebPlugin* plugin = base_proxy_->CreatePlugin(frame, params);
    if (plugin)
      return plugin;
    return Base::createPlugin(frame, params);
  }

  blink::WebScreenOrientationClient* webScreenOrientationClient() override {
    return base_proxy_->GetScreenOrientationClientMock();
  }

  void didAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override {
    base_proxy_->DidAddMessageToConsole(message, source_name, source_line);
    Base::didAddMessageToConsole(
        message, source_name, source_line, stack_trace);
  }

  bool canCreatePluginWithoutRenderer(
      const blink::WebString& mime_type) override {
    using blink::WebString;

    const CR_DEFINE_STATIC_LOCAL(
        WebString, suffix, ("-can-create-without-renderer"));
    return mime_type.utf8().find(suffix.utf8()) != std::string::npos;
  }

  void loadURLExternally(const blink::WebURLRequest& request,
                         blink::WebNavigationPolicy policy,
                         const blink::WebString& suggested_name,
                         bool replaces_current_history_item) override {
    base_proxy_->LoadURLExternally(request, policy, suggested_name,
                                   replaces_current_history_item);
    Base::loadURLExternally(request, policy, suggested_name,
                            replaces_current_history_item);
  }

  void didStartProvisionalLoad(blink::WebLocalFrame* frame,
                               double triggeringEventTime) override {
    base_proxy_->DidStartProvisionalLoad(frame);
    Base::didStartProvisionalLoad(
        frame, triggeringEventTime);
  }

  void didReceiveServerRedirectForProvisionalLoad(
      blink::WebLocalFrame* frame) override {
    base_proxy_->DidReceiveServerRedirectForProvisionalLoad(frame);
    Base::didReceiveServerRedirectForProvisionalLoad(frame);
  }

  void didFailProvisionalLoad(
      blink::WebLocalFrame* frame,
      const blink::WebURLError& error,
      blink::WebHistoryCommitType commit_type) override {
    // If the test finished, don't notify the embedder of the failed load,
    // as we already destroyed the document loader.
    if (base_proxy_->DidFailProvisionalLoad(frame, error, commit_type))
      return;
    Base::didFailProvisionalLoad(frame, error, commit_type);
  }

  void didCommitProvisionalLoad(
      blink::WebLocalFrame* frame,
      const blink::WebHistoryItem& item,
      blink::WebHistoryCommitType commit_type) override {
    base_proxy_->DidCommitProvisionalLoad(frame, item, commit_type);
    Base::didCommitProvisionalLoad(frame, item, commit_type);
  }

  void didReceiveTitle(blink::WebLocalFrame* frame,
                       const blink::WebString& title,
                       blink::WebTextDirection direction) override {
    base_proxy_->DidReceiveTitle(frame, title, direction);
    Base::didReceiveTitle(frame, title, direction);
  }

  void didChangeIcon(blink::WebLocalFrame* frame,
                     blink::WebIconURL::Type icon_type) override {
    base_proxy_->DidChangeIcon(frame, icon_type);
    Base::didChangeIcon(frame, icon_type);
  }

  void didFinishDocumentLoad(blink::WebLocalFrame* frame, bool empty) override {
    base_proxy_->DidFinishDocumentLoad(frame);
    Base::didFinishDocumentLoad(frame, empty);
  }

  void didHandleOnloadEvents(blink::WebLocalFrame* frame) override {
    base_proxy_->DidHandleOnloadEvents(frame);
    Base::didHandleOnloadEvents(frame);
  }

  void didFailLoad(blink::WebLocalFrame* frame,
                   const blink::WebURLError& error,
                   blink::WebHistoryCommitType commit_type) override {
    base_proxy_->DidFailLoad(frame, error, commit_type);
    Base::didFailLoad(frame, error, commit_type);
  }

  void didFinishLoad(blink::WebLocalFrame* frame) override {
    Base::didFinishLoad(frame);
    base_proxy_->DidFinishLoad(frame);
  }

  void didChangeSelection(bool is_selection_empty) override {
    base_proxy_->DidChangeSelection(is_selection_empty);
    Base::didChangeSelection(is_selection_empty);
  }

  blink::WebColorChooser* createColorChooser(
      blink::WebColorChooserClient* client,
      const blink::WebColor& initial_color,
      const blink::WebVector<blink::WebColorSuggestion>& suggestions) override {
    return base_proxy_->CreateColorChooser(client, initial_color, suggestions);
  }

  void runModalAlertDialog(const blink::WebString& message) override {
    base_proxy_->GetDelegate()->PrintMessage(std::string("ALERT: ") +
                                             message.utf8().data() + "\n");
  }

  bool runModalConfirmDialog(const blink::WebString& message) override {
    base_proxy_->GetDelegate()->PrintMessage(std::string("CONFIRM: ") +
                                             message.utf8().data() + "\n");
    return true;
  }

  bool runModalPromptDialog(const blink::WebString& message,
                            const blink::WebString& default_value,
                            blink::WebString*) override {
    base_proxy_->GetDelegate()->PrintMessage(
        std::string("PROMPT: ") + message.utf8().data() + ", default text: " +
        default_value.utf8().data() + "\n");
    return true;
  }

  bool runModalBeforeUnloadDialog(bool is_reload) override {
    base_proxy_->GetDelegate()->PrintMessage(
        std::string("CONFIRM NAVIGATION\n"));
    return !base_proxy_->GetInterfaces()
                ->TestRunner()
                ->ShouldStayOnPageAfterHandlingBeforeUnload();
  }

  void showContextMenu(
      const blink::WebContextMenuData& context_menu_data) override {
    base_proxy_->ShowContextMenu(context_menu_data);
    Base::showContextMenu(context_menu_data);
  }

  void didDetectXSS(const blink::WebURL& insecure_url,
                    bool did_block_entire_page) override {
    // This is not implemented in RenderFrameImpl, so need to explicitly call
    // into the base proxy.
    base_proxy_->DidDetectXSS(insecure_url, did_block_entire_page);
    Base::didDetectXSS(insecure_url, did_block_entire_page);
  }

  void didDispatchPingLoader(const blink::WebURL& url) override {
    // This is not implemented in RenderFrameImpl, so need to explicitly call
    // into the base proxy.
    base_proxy_->DidDispatchPingLoader(url);
    Base::didDispatchPingLoader(url);
  }

  void didCreateDataSource(blink::WebLocalFrame* frame,
                           blink::WebDataSource* ds) override {
    Base::didCreateDataSource(frame, ds);
  }

  void willSendRequest(
      blink::WebLocalFrame* frame,
      unsigned identifier,
      blink::WebURLRequest& request,
      const blink::WebURLResponse& redirect_response) override {
    Base::willSendRequest(frame, identifier, request, redirect_response);
    base_proxy_->WillSendRequest(frame, identifier, request, redirect_response);
  }

  void didReceiveResponse(unsigned identifier,
                          const blink::WebURLResponse& response) override {
    base_proxy_->DidReceiveResponse(identifier, response);
    Base::didReceiveResponse(identifier, response);
  }

  void didChangeResourcePriority(unsigned identifier,
                                 const blink::WebURLRequest::Priority& priority,
                                 int intra_priority_value) override {
    // This is not implemented in RenderFrameImpl, so need to explicitly call
    // into the base proxy.
    base_proxy_->DidChangeResourcePriority(
        identifier, priority, intra_priority_value);
    Base::didChangeResourcePriority(
        identifier, priority, intra_priority_value);
  }

  void didFinishResourceLoad(blink::WebLocalFrame* frame,
                             unsigned identifier) override {
    base_proxy_->DidFinishResourceLoad(frame, identifier);
  }

  blink::WebNavigationPolicy decidePolicyForNavigation(
      const blink::WebFrameClient::NavigationPolicyInfo& info) override {
    blink::WebNavigationPolicy policy = base_proxy_->DecidePolicyForNavigation(
        info);
    if (policy == blink::WebNavigationPolicyIgnore)
      return policy;

    return Base::decidePolicyForNavigation(info);
  }

  void willStartUsingPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandler* handler) override {
    // RenderFrameImpl::willStartUsingPeerConnectionHandler can not be mocked.
    // See http://crbug/363285.
  }

  blink::WebUserMediaClient* userMediaClient() override {
    return base_proxy_->GetUserMediaClient();
  }

  bool willCheckAndDispatchMessageEvent(
      blink::WebLocalFrame* source_frame,
      blink::WebFrame* target_frame,
      blink::WebSecurityOrigin target,
      blink::WebDOMMessageEvent event) override {
    if (base_proxy_->WillCheckAndDispatchMessageEvent(
            source_frame, target_frame, target, event))
      return true;
    return Base::willCheckAndDispatchMessageEvent(
        source_frame, target_frame, target, event);
  }

  void postAccessibilityEvent(const blink::WebAXObject& object,
                              blink::WebAXEvent event) override {
    base_proxy_->PostAccessibilityEvent(object, event);
    Base::postAccessibilityEvent(object, event);
  }

  void checkIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      const blink::WebSecurityOrigin& security_origin,
      blink::WebSetSinkIdCallbacks* web_callbacks) override {
    base_proxy_->CheckIfAudioSinkExistsAndIsAuthorized(sink_id, security_origin,
                                                       web_callbacks);
  }

 private:
  WebTestProxyBase* base_proxy_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameTestProxy);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_
