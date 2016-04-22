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

class AccessibilityController;
class TestRunner;
class WebTestDelegate;
class WebTestProxyBase;

// WebFrameTestClient implements WebFrameClient interface, providing behavior
// expected by tests.  WebFrameTestClient ends up used by WebFrameTestProxy
// which coordinates forwarding WebFrameClient calls either to
// WebFrameTestClient or to the product code (i.e. to RenderFrameImpl).
class WebFrameTestClient : public blink::WebFrameClient {
 public:
  // Caller has to ensure that all arguments (|test_runner|, |delegate| and so
  // forth) live longer than |this|.
  WebFrameTestClient(TestRunner* test_runner,
                     WebTestDelegate* delegate,
                     WebTestProxyBase* web_test_proxy_base);

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
  void didStartProvisionalLoad(blink::WebLocalFrame* frame,
                               double trigering_event_time) override;
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
  void didDetectXSS(const blink::WebURL& insecure_url,
                    bool did_block_entire_page) override;
  void didDispatchPingLoader(const blink::WebURL& url) override;
  void willSendRequest(blink::WebLocalFrame* frame,
                       unsigned identifier,
                       blink::WebURLRequest& request,
                       const blink::WebURLResponse& redirect_response) override;
  void didReceiveResponse(unsigned identifier,
                          const blink::WebURLResponse& response) override;
  void didChangeResourcePriority(unsigned identifier,
                                 const blink::WebURLRequest::Priority& priority,
                                 int intra_priority_value) override;
  void didFinishResourceLoad(blink::WebLocalFrame* frame,
                             unsigned identifier) override;
  blink::WebNavigationPolicy decidePolicyForNavigation(
      const blink::WebFrameClient::NavigationPolicyInfo& info) override;
  bool willCheckAndDispatchMessageEvent(
      blink::WebLocalFrame* source_frame,
      blink::WebFrame* target_frame,
      blink::WebSecurityOrigin target,
      blink::WebDOMMessageEvent event) override;
  void checkIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      const blink::WebSecurityOrigin& security_origin,
      blink::WebSetSinkIdCallbacks* web_callbacks) override;
  void didClearWindowObject(blink::WebLocalFrame* frame) override;

 private:
  // Borrowed pointers to other parts of Layout Tests state.
  TestRunner* test_runner_;
  WebTestDelegate* delegate_;
  WebTestProxyBase* web_test_proxy_base_;

  // Map from request identifier into resource url description.  The map is used
  // to track resource requests spanning willSendRequest, didReceiveResponse,
  // didChangeResourcePriority, didFinishResourceLoad.
  std::map<unsigned, std::string> resource_identifier_map_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameTestClient);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_WEB_FRAME_TEST_CLIENT_H_
