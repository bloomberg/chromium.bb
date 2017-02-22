// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_
#define CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "content/shell/test_runner/test_runner_export.h"
#include "content/shell/test_runner/web_frame_test_client.h"
#include "third_party/WebKit/public/platform/WebEffectiveConnectionType.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace test_runner {

class TEST_RUNNER_EXPORT WebFrameTestProxyBase {
 public:
  void set_test_client(std::unique_ptr<WebFrameTestClient> client) {
    DCHECK(client);
    DCHECK(!test_client_);
    test_client_ = std::move(client);
  }

  blink::WebLocalFrame* web_frame() const { return web_frame_; }
  void set_web_frame(blink::WebLocalFrame* frame) {
    DCHECK(frame);
    DCHECK(!web_frame_);
    web_frame_ = frame;
  }

 protected:
  WebFrameTestProxyBase();
  ~WebFrameTestProxyBase();
  blink::WebFrameClient* test_client() { return test_client_.get(); }

 private:
  std::unique_ptr<WebFrameTestClient> test_client_;
  blink::WebLocalFrame* web_frame_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameTestProxyBase);
};

// WebFrameTestProxy is used during LayoutTests and always instantiated, at time
// of writing with Base=RenderFrameImpl. It does not directly inherit from it
// for layering purposes.
template <class Base, typename P>
class WebFrameTestProxy : public Base, public WebFrameTestProxyBase {
 public:
  explicit WebFrameTestProxy(P p) : Base(p) {}

  virtual ~WebFrameTestProxy() {}

  // WebFrameClient implementation.
  blink::WebPlugin* createPlugin(
      blink::WebLocalFrame* frame,
      const blink::WebPluginParams& params) override {
    blink::WebPlugin* plugin = test_client()->createPlugin(frame, params);
    if (plugin)
      return plugin;
    return Base::createPlugin(frame, params);
  }

  blink::WebScreenOrientationClient* webScreenOrientationClient() override {
    return test_client()->webScreenOrientationClient();
  }

  void didAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override {
    test_client()->didAddMessageToConsole(message, source_name, source_line,
                                          stack_trace);
    Base::didAddMessageToConsole(message, source_name, source_line,
                                 stack_trace);
  }

  bool canCreatePluginWithoutRenderer(
      const blink::WebString& mime_type) override {
    const char suffix[] = "-can-create-without-renderer";
    return mime_type.utf8().find(suffix) != std::string::npos;
  }

  void loadURLExternally(const blink::WebURLRequest& request,
                         blink::WebNavigationPolicy policy,
                         const blink::WebString& suggested_name,
                         bool replaces_current_history_item) override {
    test_client()->loadURLExternally(request, policy, suggested_name,
                                     replaces_current_history_item);
    Base::loadURLExternally(request, policy, suggested_name,
                            replaces_current_history_item);
  }

  void didStartProvisionalLoad(blink::WebDataSource* data_source) override {
    test_client()->didStartProvisionalLoad(data_source);
    Base::didStartProvisionalLoad(data_source);
  }

  void didReceiveServerRedirectForProvisionalLoad(
      blink::WebLocalFrame* frame) override {
    test_client()->didReceiveServerRedirectForProvisionalLoad(frame);
    Base::didReceiveServerRedirectForProvisionalLoad(frame);
  }

  void didFailProvisionalLoad(
      blink::WebLocalFrame* frame,
      const blink::WebURLError& error,
      blink::WebHistoryCommitType commit_type) override {
    test_client()->didFailProvisionalLoad(frame, error, commit_type);
    // If the test finished, don't notify the embedder of the failed load,
    // as we already destroyed the document loader.
    if (!frame->provisionalDataSource())
      return;
    Base::didFailProvisionalLoad(frame, error, commit_type);
  }

  void didCommitProvisionalLoad(
      blink::WebLocalFrame* frame,
      const blink::WebHistoryItem& item,
      blink::WebHistoryCommitType commit_type) override {
    test_client()->didCommitProvisionalLoad(frame, item, commit_type);
    Base::didCommitProvisionalLoad(frame, item, commit_type);
  }

  void didReceiveTitle(blink::WebLocalFrame* frame,
                       const blink::WebString& title,
                       blink::WebTextDirection direction) override {
    test_client()->didReceiveTitle(frame, title, direction);
    Base::didReceiveTitle(frame, title, direction);
  }

  void didChangeIcon(blink::WebLocalFrame* frame,
                     blink::WebIconURL::Type icon_type) override {
    test_client()->didChangeIcon(frame, icon_type);
    Base::didChangeIcon(frame, icon_type);
  }

  void didFinishDocumentLoad(blink::WebLocalFrame* frame) override {
    test_client()->didFinishDocumentLoad(frame);
    Base::didFinishDocumentLoad(frame);
  }

  void didHandleOnloadEvents(blink::WebLocalFrame* frame) override {
    test_client()->didHandleOnloadEvents(frame);
    Base::didHandleOnloadEvents(frame);
  }

  void didFailLoad(blink::WebLocalFrame* frame,
                   const blink::WebURLError& error,
                   blink::WebHistoryCommitType commit_type) override {
    test_client()->didFailLoad(frame, error, commit_type);
    Base::didFailLoad(frame, error, commit_type);
  }

  void didFinishLoad(blink::WebLocalFrame* frame) override {
    Base::didFinishLoad(frame);
    test_client()->didFinishLoad(frame);
  }

  void didNavigateWithinPage(blink::WebLocalFrame* frame,
                             const blink::WebHistoryItem& history_item,
                             blink::WebHistoryCommitType commit_type,
                             bool content_initiated) override {
    Base::didNavigateWithinPage(frame, history_item, commit_type,
                                content_initiated);
    test_client()->didNavigateWithinPage(frame, history_item, commit_type,
                                         content_initiated);
  }

  void didStopLoading() override {
    Base::didStopLoading();
    test_client()->didStopLoading();
  }

  void didChangeSelection(bool is_selection_empty) override {
    test_client()->didChangeSelection(is_selection_empty);
    Base::didChangeSelection(is_selection_empty);
  }

  blink::WebColorChooser* createColorChooser(
      blink::WebColorChooserClient* client,
      const blink::WebColor& initial_color,
      const blink::WebVector<blink::WebColorSuggestion>& suggestions) override {
    return test_client()->createColorChooser(client, initial_color,
                                             suggestions);
  }

  blink::WebEffectiveConnectionType getEffectiveConnectionType() override {
    if (test_client()->getEffectiveConnectionType() !=
        blink::WebEffectiveConnectionType::TypeUnknown) {
      return test_client()->getEffectiveConnectionType();
    }
    return Base::getEffectiveConnectionType();
  }

  void runModalAlertDialog(const blink::WebString& message) override {
    test_client()->runModalAlertDialog(message);
  }

  bool runModalConfirmDialog(const blink::WebString& message) override {
    return test_client()->runModalConfirmDialog(message);
  }

  bool runModalPromptDialog(const blink::WebString& message,
                            const blink::WebString& default_value,
                            blink::WebString* actual_value) override {
    return test_client()->runModalPromptDialog(message, default_value,
                                               actual_value);
  }

  bool runModalBeforeUnloadDialog(bool is_reload) override {
    return test_client()->runModalBeforeUnloadDialog(is_reload);
  }

  void showContextMenu(
      const blink::WebContextMenuData& context_menu_data) override {
    test_client()->showContextMenu(context_menu_data);
    Base::showContextMenu(context_menu_data);
  }

  void didDetectXSS(const blink::WebURL& insecure_url,
                    bool did_block_entire_page) override {
    // This is not implemented in RenderFrameImpl, so need to explicitly call
    // into the base proxy.
    test_client()->didDetectXSS(insecure_url, did_block_entire_page);
    Base::didDetectXSS(insecure_url, did_block_entire_page);
  }

  void didDispatchPingLoader(const blink::WebURL& url) override {
    // This is not implemented in RenderFrameImpl, so need to explicitly call
    // into the base proxy.
    test_client()->didDispatchPingLoader(url);
    Base::didDispatchPingLoader(url);
  }

  void willSendRequest(blink::WebLocalFrame* frame,
                       blink::WebURLRequest& request) override {
    Base::willSendRequest(frame, request);
    test_client()->willSendRequest(frame, request);
  }

  void didReceiveResponse(const blink::WebURLResponse& response) override {
    test_client()->didReceiveResponse(response);
    Base::didReceiveResponse(response);
  }

  blink::WebNavigationPolicy decidePolicyForNavigation(
      const blink::WebFrameClient::NavigationPolicyInfo& info) override {
    blink::WebNavigationPolicy policy =
        test_client()->decidePolicyForNavigation(info);
    if (policy == blink::WebNavigationPolicyIgnore)
      return policy;

    return Base::decidePolicyForNavigation(info);
  }

  void didStartLoading(bool to_different_document) override {
    Base::didStartLoading(to_different_document);
    test_client()->didStartLoading(to_different_document);
  }

  void willStartUsingPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandler* handler) override {
    // RenderFrameImpl::willStartUsingPeerConnectionHandler can not be mocked.
    // See http://crbug/363285.
  }

  blink::WebUserMediaClient* userMediaClient() override {
    return test_client()->userMediaClient();
  }

  void postAccessibilityEvent(const blink::WebAXObject& object,
                              blink::WebAXEvent event) override {
    test_client()->postAccessibilityEvent(object, event);
    Base::postAccessibilityEvent(object, event);
  }

  void checkIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      const blink::WebSecurityOrigin& security_origin,
      blink::WebSetSinkIdCallbacks* web_callbacks) override {
    test_client()->checkIfAudioSinkExistsAndIsAuthorized(
        sink_id, security_origin, web_callbacks);
  }

  void didClearWindowObject(blink::WebLocalFrame* frame) override {
    test_client()->didClearWindowObject(frame);
    Base::didClearWindowObject(frame);
  }
  bool runFileChooser(const blink::WebFileChooserParams& params,
                      blink::WebFileChooserCompletion* completion) override {
    return test_client()->runFileChooser(params, completion);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebFrameTestProxy);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_
