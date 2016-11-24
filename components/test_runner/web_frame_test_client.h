// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_WEB_FRAME_TEST_CLIENT_H_
#define COMPONENTS_TEST_RUNNER_WEB_FRAME_TEST_CLIENT_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"

namespace test_runner {

class TestRunner;
class WebFrameTestProxyBase;
class WebTestDelegate;
class WebViewTestProxyBase;

// WebFrameTestClient implements WebFrameClient interface, providing behavior
// expected by tests.  WebFrameTestClient ends up used by WebFrameTestProxy
// which coordinates forwarding WebFrameClient calls either to
// WebFrameTestClient or to the product code (i.e. to RenderFrameImpl).
class WebFrameTestClient : public blink::WebFrameClient {
 public:
  // Caller has to ensure that all arguments (|delegate|,
  // |web_view_test_proxy_base_| and so forth) live longer than |this|.
  WebFrameTestClient(WebTestDelegate* delegate,
                     WebViewTestProxyBase* web_view_test_proxy_base,
                     WebFrameTestProxyBase* web_frame_test_proxy_base);

  ~WebFrameTestClient() override;

  // WebFrameClient overrides needed by WebFrameTestProxy.
  blink::WebColorChooser* createColorChooser(
      blink::WebColorChooserClient* client,
      const blink::WebColor& initial_color,
      const blink::WebVector<blink::WebColorSuggestion>& suggestions) override;
  void runModalAlertDialog(const blink::WebString& message) override;
  bool runModalConfirmDialog(const blink::WebString& message) override;
  bool runModalPromptDialog(const blink::WebString& message,
                            const blink::WebString& default_value,
                            blink::WebString* actual_value) override;
  bool runModalBeforeUnloadDialog(bool is_reload) override;
  blink::WebScreenOrientationClient* webScreenOrientationClient() override;
  void postAccessibilityEvent(const blink::WebAXObject& object,
                              blink::WebAXEvent event) override;
  void didChangeSelection(bool is_selection_empty) override;
  blink::WebPlugin* createPlugin(blink::WebLocalFrame* frame,
                                 const blink::WebPluginParams& params) override;
  void showContextMenu(
      const blink::WebContextMenuData& context_menu_data) override;
  blink::WebUserMediaClient* userMediaClient() override;
  void didAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override;
  void loadURLExternally(const blink::WebURLRequest& request,
                         blink::WebNavigationPolicy policy,
                         const blink::WebString& suggested_name,
                         bool replaces_current_history_item) override;
  void didStartProvisionalLoad(blink::WebLocalFrame* frame) override;
  void didReceiveServerRedirectForProvisionalLoad(
      blink::WebLocalFrame* frame) override;
  void didFailProvisionalLoad(blink::WebLocalFrame* frame,
                              const blink::WebURLError& error,
                              blink::WebHistoryCommitType commit_type) override;
  void didCommitProvisionalLoad(
      blink::WebLocalFrame* frame,
      const blink::WebHistoryItem& history_item,
      blink::WebHistoryCommitType history_type) override;
  void didReceiveTitle(blink::WebLocalFrame* frame,
                       const blink::WebString& title,
                       blink::WebTextDirection direction) override;
  void didChangeIcon(blink::WebLocalFrame* frame,
                     blink::WebIconURL::Type icon_type) override;
  void didFinishDocumentLoad(blink::WebLocalFrame* frame) override;
  void didHandleOnloadEvents(blink::WebLocalFrame* frame) override;
  void didFailLoad(blink::WebLocalFrame* frame,
                   const blink::WebURLError& error,
                   blink::WebHistoryCommitType commit_type) override;
  void didFinishLoad(blink::WebLocalFrame* frame) override;
  void didNavigateWithinPage(blink::WebLocalFrame* frame,
                             const blink::WebHistoryItem& history_item,
                             blink::WebHistoryCommitType commit_type,
                             bool contentInitiated) override;
  void didStartLoading(bool to_different_document) override;
  void didStopLoading() override;
  void didDetectXSS(const blink::WebURL& insecure_url,
                    bool did_block_entire_page) override;
  void didDispatchPingLoader(const blink::WebURL& url) override;
  void willSendRequest(blink::WebLocalFrame* frame,
                       blink::WebURLRequest& request) override;
  void didReceiveResponse(const blink::WebURLResponse& response) override;
  blink::WebNavigationPolicy decidePolicyForNavigation(
      const blink::WebFrameClient::NavigationPolicyInfo& info) override;
  void checkIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      const blink::WebSecurityOrigin& security_origin,
      blink::WebSetSinkIdCallbacks* web_callbacks) override;
  void didClearWindowObject(blink::WebLocalFrame* frame) override;
  bool runFileChooser(const blink::WebFileChooserParams& params,
                      blink::WebFileChooserCompletion* completion) override;
  blink::WebEffectiveConnectionType getEffectiveConnectionType() override;

 private:
  TestRunner* test_runner();

  // Borrowed pointers to other parts of Layout Tests state.
  WebTestDelegate* delegate_;
  WebViewTestProxyBase* web_view_test_proxy_base_;
  WebFrameTestProxyBase* web_frame_test_proxy_base_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameTestClient);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_WEB_FRAME_TEST_CLIENT_H_
