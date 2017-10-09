// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui.h"

namespace IPC {
class Message;
}

namespace content {
class RenderFrameHost;

class CONTENT_EXPORT WebUIImpl : public WebUI,
                                 public base::SupportsWeakPtr<WebUIImpl> {
 public:
  WebUIImpl(WebContents* contents);
  ~WebUIImpl() override;

  // Called when a RenderFrame is created for a WebUI (reload after a renderer
  // crash) or when a WebUI is created for a RenderFrame (i.e. navigating from
  // chrome://downloads to chrome://bookmarks) or when both are new (i.e.
  // opening a new tab).
  void RenderFrameCreated(RenderFrameHost* render_frame_host);

  // Called when a RenderFrame is reused for the same WebUI type (i.e. reload).
  void RenderFrameReused(RenderFrameHost* render_frame_host);

  // Called when the owning RenderFrameHost has started swapping out.
  void RenderFrameHostSwappingOut();

  // WebUI implementation:
  WebContents* GetWebContents() const override;
  WebUIController* GetController() const override;
  void SetController(WebUIController* controller) override;
  float GetDeviceScaleFactor() const override;
  const base::string16& GetOverriddenTitle() const override;
  void OverrideTitle(const base::string16& title) override;
  int GetBindings() const override;
  void SetBindings(int bindings) override;
  void AddMessageHandler(std::unique_ptr<WebUIMessageHandler> handler) override;
  typedef base::Callback<void(const base::ListValue*)> MessageCallback;
  void RegisterMessageCallback(const std::string& message,
                               const MessageCallback& callback) override;
  void ProcessWebUIMessage(const GURL& source_url,
                           const std::string& message,
                           const base::ListValue& args) override;
  bool CanCallJavascript() override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name) override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                    const base::Value& arg) override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                    const base::Value& arg1,
                                    const base::Value& arg2) override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                    const base::Value& arg1,
                                    const base::Value& arg2,
                                    const base::Value& arg3) override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                    const base::Value& arg1,
                                    const base::Value& arg2,
                                    const base::Value& arg3,
                                    const base::Value& arg4) override;
  void CallJavascriptFunctionUnsafe(
      const std::string& function_name,
      const std::vector<const base::Value*>& args) override;
  std::vector<std::unique_ptr<WebUIMessageHandler>>* GetHandlersForTesting()
      override;

  bool OnMessageReceived(const IPC::Message& message, RenderFrameHost* sender);

 private:
  class MainFrameNavigationObserver;

  // IPC message handling.
  void OnWebUISend(RenderFrameHost* sender,
                   const GURL& source_url,
                   const std::string& message,
                   const base::ListValue& args);

  // Execute a string of raw JavaScript on the page.
  void ExecuteJavascript(const base::string16& javascript);

  // Called internally and by the owned MainFrameNavigationObserver.
  void DisallowJavascriptOnAllHandlers();

  // A map of message name -> message handling callback.
  typedef std::map<std::string, MessageCallback> MessageCallbackMap;
  MessageCallbackMap message_callbacks_;

  // Options that may be overridden by individual Web UI implementations. The
  // bool options default to false. See the public getters for more information.
  base::string16 overridden_title_;  // Defaults to empty string.
  int bindings_;  // The bindings from BindingsPolicy that should be enabled for
                  // this page.

  // The WebUIMessageHandlers we own.
  std::vector<std::unique_ptr<WebUIMessageHandler>> handlers_;

  // Non-owning pointer to the WebContents this WebUI is associated with.
  WebContents* web_contents_;

  // Notifies this WebUI about notifications in the main frame.
  std::unique_ptr<MainFrameNavigationObserver> web_contents_observer_;

  std::unique_ptr<WebUIController> controller_;

  DISALLOW_COPY_AND_ASSIGN(WebUIImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_
