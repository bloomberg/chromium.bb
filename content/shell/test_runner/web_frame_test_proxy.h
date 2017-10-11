// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_
#define CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "content/public/common/content_switches.h"
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
  blink::WebPlugin* CreatePlugin(
      const blink::WebPluginParams& params) override {
    blink::WebPlugin* plugin = test_client()->CreatePlugin(params);
    if (plugin)
      return plugin;
    return Base::CreatePlugin(params);
  }

  blink::WebScreenOrientationClient* GetWebScreenOrientationClient() override {
    return test_client()->GetWebScreenOrientationClient();
  }

  void DidAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override {
    test_client()->DidAddMessageToConsole(message, source_name, source_line,
                                          stack_trace);
    Base::DidAddMessageToConsole(message, source_name, source_line,
                                 stack_trace);
  }

  bool CanCreatePluginWithoutRenderer(
      const blink::WebString& mime_type) override {
    const char suffix[] = "-can-create-without-renderer";
    return mime_type.Utf8().find(suffix) != std::string::npos;
  }

  void DownloadURL(const blink::WebURLRequest& request,
                   const blink::WebString& suggested_name) override {
    test_client()->DownloadURL(request, suggested_name);
    Base::DownloadURL(request, suggested_name);
  }


  void DidStartProvisionalLoad(blink::WebDocumentLoader* document_loader,
                               blink::WebURLRequest& request) override {
    test_client()->DidStartProvisionalLoad(document_loader, request);
    Base::DidStartProvisionalLoad(document_loader, request);
  }

  void DidReceiveServerRedirectForProvisionalLoad() override {
    test_client()->DidReceiveServerRedirectForProvisionalLoad();
    Base::DidReceiveServerRedirectForProvisionalLoad();
  }

  void DidFailProvisionalLoad(
      const blink::WebURLError& error,
      blink::WebHistoryCommitType commit_type) override {
    test_client()->DidFailProvisionalLoad(error, commit_type);
    // If the test finished, don't notify the embedder of the failed load,
    // as we already destroyed the document loader.
    if (!web_frame()->GetProvisionalDocumentLoader())
      return;
    Base::DidFailProvisionalLoad(error, commit_type);
  }

  void DidCommitProvisionalLoad(
      const blink::WebHistoryItem& item,
      blink::WebHistoryCommitType commit_type) override {
    test_client()->DidCommitProvisionalLoad(item, commit_type);
    Base::DidCommitProvisionalLoad(item, commit_type);
  }

  void DidReceiveTitle(const blink::WebString& title,
                       blink::WebTextDirection direction) override {
    test_client()->DidReceiveTitle(title, direction);
    Base::DidReceiveTitle(title, direction);
  }

  void DidChangeIcon(blink::WebIconURL::Type icon_type) override {
    test_client()->DidChangeIcon(icon_type);
    Base::DidChangeIcon(icon_type);
  }

  void DidFinishDocumentLoad() override {
    test_client()->DidFinishDocumentLoad();
    Base::DidFinishDocumentLoad();
  }

  void DidHandleOnloadEvents() override {
    test_client()->DidHandleOnloadEvents();
    Base::DidHandleOnloadEvents();
  }

  void DidFailLoad(const blink::WebURLError& error,
                   blink::WebHistoryCommitType commit_type) override {
    test_client()->DidFailLoad(error, commit_type);
    Base::DidFailLoad(error, commit_type);
  }

  void DidFinishLoad() override {
    Base::DidFinishLoad();
    test_client()->DidFinishLoad();
  }

  void DidStopLoading() override {
    Base::DidStopLoading();
    test_client()->DidStopLoading();
  }

  void DidChangeSelection(bool is_selection_empty) override {
    test_client()->DidChangeSelection(is_selection_empty);
    Base::DidChangeSelection(is_selection_empty);
  }

  blink::WebColorChooser* CreateColorChooser(
      blink::WebColorChooserClient* client,
      const blink::WebColor& initial_color,
      const blink::WebVector<blink::WebColorSuggestion>& suggestions) override {
    return test_client()->CreateColorChooser(client, initial_color,
                                             suggestions);
  }

  blink::WebEffectiveConnectionType GetEffectiveConnectionType() override {
    if (test_client()->GetEffectiveConnectionType() !=
        blink::WebEffectiveConnectionType::kTypeUnknown) {
      return test_client()->GetEffectiveConnectionType();
    }
    return Base::GetEffectiveConnectionType();
  }

  void RunModalAlertDialog(const blink::WebString& message) override {
    test_client()->RunModalAlertDialog(message);
  }

  bool RunModalConfirmDialog(const blink::WebString& message) override {
    return test_client()->RunModalConfirmDialog(message);
  }

  bool RunModalPromptDialog(const blink::WebString& message,
                            const blink::WebString& default_value,
                            blink::WebString* actual_value) override {
    return test_client()->RunModalPromptDialog(message, default_value,
                                               actual_value);
  }

  bool RunModalBeforeUnloadDialog(bool is_reload) override {
    return test_client()->RunModalBeforeUnloadDialog(is_reload);
  }

  void ShowContextMenu(
      const blink::WebContextMenuData& context_menu_data) override {
    test_client()->ShowContextMenu(context_menu_data);
    Base::ShowContextMenu(context_menu_data);
  }

  void DidDetectXSS(const blink::WebURL& insecure_url,
                    bool did_block_entire_page) override {
    // This is not implemented in RenderFrameImpl, so need to explicitly call
    // into the base proxy.
    test_client()->DidDetectXSS(insecure_url, did_block_entire_page);
    Base::DidDetectXSS(insecure_url, did_block_entire_page);
  }

  void DidDispatchPingLoader(const blink::WebURL& url) override {
    // This is not implemented in RenderFrameImpl, so need to explicitly call
    // into the base proxy.
    test_client()->DidDispatchPingLoader(url);
    Base::DidDispatchPingLoader(url);
  }

  void WillSendRequest(blink::WebURLRequest& request) override {
    Base::WillSendRequest(request);
    test_client()->WillSendRequest(request);
  }

  void DidReceiveResponse(const blink::WebURLResponse& response) override {
    test_client()->DidReceiveResponse(response);
    Base::DidReceiveResponse(response);
  }

  blink::WebNavigationPolicy DecidePolicyForNavigation(
      const blink::WebFrameClient::NavigationPolicyInfo& info) override {
    blink::WebNavigationPolicy policy =
        test_client()->DecidePolicyForNavigation(info);
    if (policy == blink::kWebNavigationPolicyIgnore)
      return policy;

    return Base::DecidePolicyForNavigation(info);
  }

  blink::WebUserMediaClient* UserMediaClient() override {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kUseFakeUIForMediaStream)) {
      return Base::UserMediaClient();
    }
    return test_client()->UserMediaClient();
  }

  void PostAccessibilityEvent(const blink::WebAXObject& object,
                              blink::WebAXEvent event) override {
    test_client()->PostAccessibilityEvent(object, event);
    Base::PostAccessibilityEvent(object, event);
  }

  void CheckIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      const blink::WebSecurityOrigin& security_origin,
      blink::WebSetSinkIdCallbacks* web_callbacks) override {
    test_client()->CheckIfAudioSinkExistsAndIsAuthorized(
        sink_id, security_origin, web_callbacks);
  }

  void DidClearWindowObject() override {
    test_client()->DidClearWindowObject();
    Base::DidClearWindowObject();
  }
  bool RunFileChooser(const blink::WebFileChooserParams& params,
                      blink::WebFileChooserCompletion* completion) override {
    return test_client()->RunFileChooser(params, completion);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebFrameTestProxy);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_
