// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui.h"
#include "ipc/ipc_listener.h"

namespace content {
class RenderFrameHost;
class RenderViewHost;

class CONTENT_EXPORT WebUIImpl : public WebUI,
                                 public IPC::Listener,
                                 public base::SupportsWeakPtr<WebUIImpl> {
 public:
  explicit WebUIImpl(WebContents* contents);
  ~WebUIImpl() override;

  // Called by WebContentsImpl when the RenderView is first created. This is
  // *not* called for every page load because in some cases
  // RenderFrameHostManager will reuse RenderView instances.
  void RenderViewCreated(RenderViewHost* render_view_host);

  // WebUI implementation:
  WebContents* GetWebContents() const override;
  WebUIController* GetController() const override;
  void SetController(WebUIController* controller) override;
  float GetDeviceScaleFactor() const override;
  const base::string16& GetOverriddenTitle() const override;
  void OverrideTitle(const base::string16& title) override;
  ui::PageTransition GetLinkTransitionType() const override;
  void SetLinkTransitionType(ui::PageTransition type) override;
  int GetBindings() const override;
  void SetBindings(int bindings) override;
  void OverrideJavaScriptFrame(const std::string& frame_name) override;
  void AddMessageHandler(WebUIMessageHandler* handler) override;
  typedef base::Callback<void(const base::ListValue*)> MessageCallback;
  void RegisterMessageCallback(const std::string& message,
                               const MessageCallback& callback) override;
  void ProcessWebUIMessage(const GURL& source_url,
                           const std::string& message,
                           const base::ListValue& args) override;
  void CallJavascriptFunction(const std::string& function_name) override;
  void CallJavascriptFunction(const std::string& function_name,
                              const base::Value& arg) override;
  void CallJavascriptFunction(const std::string& function_name,
                              const base::Value& arg1,
                              const base::Value& arg2) override;
  void CallJavascriptFunction(const std::string& function_name,
                              const base::Value& arg1,
                              const base::Value& arg2,
                              const base::Value& arg3) override;
  void CallJavascriptFunction(const std::string& function_name,
                              const base::Value& arg1,
                              const base::Value& arg2,
                              const base::Value& arg3,
                              const base::Value& arg4) override;
  void CallJavascriptFunction(
      const std::string& function_name,
      const std::vector<const base::Value*>& args) override;

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  // IPC message handling.
  void OnWebUISend(const GURL& source_url,
                   const std::string& message,
                   const base::ListValue& args);

  // Execute a string of raw JavaScript on the page.
  void ExecuteJavascript(const base::string16& javascript);

  // Finds the frame in which to execute JavaScript (the one specified by
  // OverrideJavaScriptFrame). May return NULL if no frame of the specified name
  // exists in the page.
  RenderFrameHost* TargetFrame();

  // A helper function for TargetFrame; adds a frame to the specified set if its
  // name matches frame_name_.
  void AddToSetIfFrameNameMatches(std::set<RenderFrameHost*>* frame_set,
                                  RenderFrameHost* host);

  // A map of message name -> message handling callback.
  typedef std::map<std::string, MessageCallback> MessageCallbackMap;
  MessageCallbackMap message_callbacks_;

  // Options that may be overridden by individual Web UI implementations. The
  // bool options default to false. See the public getters for more information.
  base::string16 overridden_title_;  // Defaults to empty string.
  ui::PageTransition link_transition_type_;  // Defaults to LINK.
  int bindings_;  // The bindings from BindingsPolicy that should be enabled for
                  // this page.

  // The WebUIMessageHandlers we own.
  ScopedVector<WebUIMessageHandler> handlers_;

  // Non-owning pointer to the WebContents this WebUI is associated with.
  WebContents* web_contents_;

  // The name of the iframe this WebUI is embedded in (empty if not explicitly
  // overridden with OverrideJavaScriptFrame).
  std::string frame_name_;

  scoped_ptr<WebUIController> controller_;

  DISALLOW_COPY_AND_ASSIGN(WebUIImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_
