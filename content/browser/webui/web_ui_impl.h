// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui.h"
#include "ipc/ipc_listener.h"

namespace content {
class RenderViewHost;
}

class CONTENT_EXPORT WebUIImpl : public content::WebUI,
                                 public IPC::Listener,
                                 public base::SupportsWeakPtr<WebUIImpl> {
 public:
  explicit WebUIImpl(content::WebContents* contents);
  virtual ~WebUIImpl();

  // Called by WebContentsImpl when the RenderView is first created. This is
  // *not* called for every page load because in some cases
  // RenderViewHostManager will reuse RenderView instances.
  void RenderViewCreated(content::RenderViewHost* render_view_host);

  // WebUI implementation:
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual content::WebUIController* GetController() const OVERRIDE;
  virtual void SetController(content::WebUIController* controller) OVERRIDE;
  virtual float GetDeviceScale() const OVERRIDE;
  virtual bool ShouldHideFavicon() const OVERRIDE;
  virtual void HideFavicon() OVERRIDE;
  virtual bool ShouldFocusLocationBarByDefault() const OVERRIDE;
  virtual void FocusLocationBarByDefault() OVERRIDE;
  virtual bool ShouldHideURL() const OVERRIDE;
  virtual void HideURL() OVERRIDE;
  virtual const string16& GetOverriddenTitle() const OVERRIDE;
  virtual void OverrideTitle(const string16& title) OVERRIDE;
  virtual content::PageTransition GetLinkTransitionType() const OVERRIDE;
  virtual void SetLinkTransitionType(content::PageTransition type) OVERRIDE;
  virtual int GetBindings() const OVERRIDE;
  virtual void SetBindings(int bindings) OVERRIDE;
  virtual void SetFrameXPath(const std::string& xpath) OVERRIDE;
  virtual void AddMessageHandler(
      content::WebUIMessageHandler* handler) OVERRIDE;
  typedef base::Callback<void(const base::ListValue*)> MessageCallback;
  virtual void RegisterMessageCallback(
      const std::string& message,
      const MessageCallback& callback) OVERRIDE;
  virtual void ProcessWebUIMessage(const GURL& source_url,
                                   const std::string& message,
                                   const base::ListValue& args) OVERRIDE;
  virtual void CallJavascriptFunction(
      const std::string& function_name) OVERRIDE;
  virtual void CallJavascriptFunction(const std::string& function_name,
                                      const base::Value& arg) OVERRIDE;
  virtual void CallJavascriptFunction(const std::string& function_name,
                                      const base::Value& arg1,
                                      const base::Value& arg2) OVERRIDE;
  virtual void CallJavascriptFunction(const std::string& function_name,
                                      const base::Value& arg1,
                                      const base::Value& arg2,
                                      const base::Value& arg3) OVERRIDE;
  virtual void CallJavascriptFunction(const std::string& function_name,
                                      const base::Value& arg1,
                                      const base::Value& arg2,
                                      const base::Value& arg3,
                                      const base::Value& arg4) OVERRIDE;
  virtual void CallJavascriptFunction(
      const std::string& function_name,
      const std::vector<const base::Value*>& args) OVERRIDE;

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // IPC message handling.
  void OnWebUISend(const GURL& source_url,
                   const std::string& message,
                   const base::ListValue& args);

  // Execute a string of raw Javascript on the page.
  void ExecuteJavascript(const string16& javascript);

  // A map of message name -> message handling callback.
  typedef std::map<std::string, MessageCallback> MessageCallbackMap;
  MessageCallbackMap message_callbacks_;

  // Options that may be overridden by individual Web UI implementations. The
  // bool options default to false. See the public getters for more information.
  bool hide_favicon_;
  bool focus_location_bar_by_default_;
  bool should_hide_url_;
  string16 overridden_title_;  // Defaults to empty string.
  content::PageTransition link_transition_type_;  // Defaults to LINK.
  int bindings_;  // The bindings from BindingsPolicy that should be enabled for
                  // this page.

  // The WebUIMessageHandlers we own.
  std::vector<content::WebUIMessageHandler*> handlers_;

  // Non-owning pointer to the WebContents this WebUI is associated with.
  content::WebContents* web_contents_;

  // The path for the iframe this WebUI is embedded in (empty if not in an
  // iframe).
  std::string frame_xpath_;

  scoped_ptr<content::WebUIController> controller_;

  DISALLOW_COPY_AND_ASSIGN(WebUIImpl);
};

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_
