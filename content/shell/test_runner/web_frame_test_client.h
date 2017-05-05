// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_CLIENT_H_
#define CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_CLIENT_H_

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
  blink::WebColorChooser* CreateColorChooser(
      blink::WebColorChooserClient* client,
      const blink::WebColor& initial_color,
      const blink::WebVector<blink::WebColorSuggestion>& suggestions) override;
  void RunModalAlertDialog(const blink::WebString& message) override;
  bool RunModalConfirmDialog(const blink::WebString& message) override;
  bool RunModalPromptDialog(const blink::WebString& message,
                            const blink::WebString& default_value,
                            blink::WebString* actual_value) override;
  bool RunModalBeforeUnloadDialog(bool is_reload) override;
  blink::WebScreenOrientationClient* GetWebScreenOrientationClient() override;
  void PostAccessibilityEvent(const blink::WebAXObject& object,
                              blink::WebAXEvent event) override;
  void DidChangeSelection(bool is_selection_empty) override;
  blink::WebPlugin* CreatePlugin(const blink::WebPluginParams& params) override;
  void ShowContextMenu(
      const blink::WebContextMenuData& context_menu_data) override;
  blink::WebUserMediaClient* UserMediaClient() override;
  void DidAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override;
  void LoadURLExternally(const blink::WebURLRequest& request,
                         blink::WebNavigationPolicy policy,
                         const blink::WebString& suggested_name,
                         bool replaces_current_history_item) override;
  void LoadErrorPage(int reason) override;
  void DidStartProvisionalLoad(blink::WebDataSource* data_source,
                               blink::WebURLRequest& request) override;
  void DidReceiveServerRedirectForProvisionalLoad() override;
  void DidFailProvisionalLoad(const blink::WebURLError& error,
                              blink::WebHistoryCommitType commit_type) override;
  void DidCommitProvisionalLoad(
      const blink::WebHistoryItem& history_item,
      blink::WebHistoryCommitType history_type) override;
  void DidReceiveTitle(const blink::WebString& title,
                       blink::WebTextDirection direction) override;
  void DidChangeIcon(blink::WebIconURL::Type icon_type) override;
  void DidFinishDocumentLoad() override;
  void DidHandleOnloadEvents() override;
  void DidFailLoad(const blink::WebURLError& error,
                   blink::WebHistoryCommitType commit_type) override;
  void DidFinishLoad() override;
  void DidNavigateWithinPage(const blink::WebHistoryItem& history_item,
                             blink::WebHistoryCommitType commit_type,
                             bool contentInitiated) override;
  void DidStartLoading(bool to_different_document) override;
  void DidStopLoading() override;
  void DidDetectXSS(const blink::WebURL& insecure_url,
                    bool did_block_entire_page) override;
  void DidDispatchPingLoader(const blink::WebURL& url) override;
  void WillSendRequest(blink::WebURLRequest& request) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  blink::WebNavigationPolicy DecidePolicyForNavigation(
      const blink::WebFrameClient::NavigationPolicyInfo& info) override;
  void CheckIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      const blink::WebSecurityOrigin& security_origin,
      blink::WebSetSinkIdCallbacks* web_callbacks) override;
  void DidClearWindowObject() override;
  bool RunFileChooser(const blink::WebFileChooserParams& params,
                      blink::WebFileChooserCompletion* completion) override;
  blink::WebEffectiveConnectionType GetEffectiveConnectionType() override;

 private:
  TestRunner* test_runner();

  // Borrowed pointers to other parts of Layout Tests state.
  WebTestDelegate* delegate_;
  WebViewTestProxyBase* web_view_test_proxy_base_;
  WebFrameTestProxyBase* web_frame_test_proxy_base_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameTestClient);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_CLIENT_H_
