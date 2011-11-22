// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_old.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "content/common/content_export.h"
#include "content/public/common/page_transition_types.h"
#include "ipc/ipc_channel.h"

class GURL;
class RenderViewHost;
class TabContents;
class WebUIMessageHandler;

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

// A WebUI sets up the datasources and message handlers for a given HTML-based
// UI.
//
// NOTE: If you're creating a new WebUI for Chrome code, make sure you extend
// ChromeWebUI.
class CONTENT_EXPORT WebUI : public IPC::Channel::Listener {
 public:
  explicit WebUI(TabContents* contents);
  virtual ~WebUI();

  // IPC message handling.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnWebUISend(const GURL& source_url,
                           const std::string& message,
                           const base::ListValue& args);

  // Called by RenderViewHost when the RenderView is first created. This is
  // *not* called for every page load because in some cases
  // RenderViewHostManager will reuse RenderView instances. In those cases,
  // RenderViewReused will be called instead.
  virtual void RenderViewCreated(RenderViewHost* render_view_host);

  // Called by RenderViewHostManager when a RenderView is reused to display a
  // page.
  virtual void RenderViewReused(RenderViewHost* render_view_host) {}

  // Called when this becomes the active WebUI instance for a re-used
  // RenderView; this is the point at which this WebUI instance will receive
  // DOM messages instead of the previous WebUI instance.
  //
  // If a WebUI instance has code that is usually triggered from a JavaScript
  // onload handler, this should be overridden to check to see if the web page's
  // DOM is still intact (e.g., due to a back/forward navigation that remains
  // within the same page), and if so trigger that code manually since onload
  // won't be run in that case.
  virtual void DidBecomeActiveForReusedRenderView() {}

  // Used by WebUIMessageHandlers. If the given message is already registered,
  // the call has no effect unless |register_callback_overwrites_| is set to
  // true.
  typedef base::Callback<void(const base::ListValue*)> MessageCallback;
  void RegisterMessageCallback(const std::string& message,
                               const MessageCallback& callback);

  // Returns true if the favicon should be hidden for the current tab.
  bool hide_favicon() const {
    return hide_favicon_;
  }

  // Returns true if the location bar should be focused by default rather than
  // the page contents. Some pages will want to use this to encourage the user
  // to type in the URL bar.
  bool focus_location_bar_by_default() const {
    return focus_location_bar_by_default_;
  }

  // Returns true if the page's URL should be hidden. Some Web UI pages
  // like the new tab page will want to hide it.
  bool should_hide_url() const {
    return should_hide_url_;
  }

  // Gets a custom tab title provided by the Web UI. If there is no title
  // override, the string will be empty which should trigger the default title
  // behavior for the tab.
  const string16& overridden_title() const {
    return overridden_title_;
  }

  // Returns the transition type that should be used for link clicks on this
  // Web UI. This will default to LINK but may be overridden.
  content::PageTransition link_transition_type() const {
    return link_transition_type_;
  }

  int bindings() const {
    return bindings_;
  }

  // Indicates whether RegisterMessageCallback() will overwrite an existing
  // message callback mapping.  Serves as the hook for test mocks.
  bool register_callback_overwrites() const {
    return register_callback_overwrites_;
  }

  void set_register_callback_overwrites(bool value) {
    register_callback_overwrites_ = value;
  }

  // Call a Javascript function by sending its name and arguments down to
  // the renderer.  This is asynchronous; there's no way to get the result
  // of the call, and should be thought of more like sending a message to
  // the page.
  // All function names in WebUI must consist of only ASCII characters.
  // There are variants for calls with more arguments.
  void CallJavascriptFunction(const std::string& function_name);
  void CallJavascriptFunction(const std::string& function_name,
                              const base::Value& arg);
  void CallJavascriptFunction(const std::string& function_name,
                              const base::Value& arg1,
                              const base::Value& arg2);
  void CallJavascriptFunction(const std::string& function_name,
                              const base::Value& arg1,
                              const base::Value& arg2,
                              const base::Value& arg3);
  void CallJavascriptFunction(const std::string& function_name,
                              const base::Value& arg1,
                              const base::Value& arg2,
                              const base::Value& arg3,
                              const base::Value& arg4);
  void CallJavascriptFunction(const std::string& function_name,
                              const std::vector<const base::Value*>& args);

  TabContents* tab_contents() const { return tab_contents_; }

  // An opaque identifier used to identify a WebUI. This can only be compared to
  // kNoWebUI or other WebUI types. See GetWebUIType.
  typedef void* TypeID;

  // A special WebUI type that signifies that a given page would not use the
  // Web UI system.
  static const TypeID kNoWebUI;

  // Returns JavaScript code that, when executed, calls the function specified
  // by |function_name| with the arguments specified in |arg_list|.
  static string16 GetJavascriptCall(
      const std::string& function_name,
      const std::vector<const base::Value*>& arg_list);

 protected:
  // Takes ownership of |handler|, which will be destroyed when the WebUI is.
  void AddMessageHandler(WebUIMessageHandler* handler);

  // Execute a string of raw Javascript on the page.  Overridable for
  // testing purposes.
  virtual void ExecuteJavascript(const string16& javascript);

  // Options that may be overridden by individual Web UI implementations. The
  // bool options default to false. See the public getters for more information.
  bool hide_favicon_;
  bool focus_location_bar_by_default_;
  bool should_hide_url_;
  string16 overridden_title_;  // Defaults to empty string.
  content::PageTransition link_transition_type_;  // Defaults to LINK.
  int bindings_;  // The bindings from BindingsPolicy that should be enabled for
                  // this page.

  // Used by test mocks. See the public getters for more information.
  bool register_callback_overwrites_;  // Defaults to false.

  // The WebUIMessageHandlers we own.
  std::vector<WebUIMessageHandler*> handlers_;

  // Non-owning pointer to the TabContents this WebUI is associated with.
  TabContents* tab_contents_;

 private:
  // A map of message name -> message handling callback.
  typedef std::map<std::string, MessageCallback> MessageCallbackMap;
  MessageCallbackMap message_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(WebUI);
};

// Messages sent from the DOM are forwarded via the WebUI to handler
// classes. These objects are owned by WebUI and destroyed when the
// host is destroyed.
class CONTENT_EXPORT WebUIMessageHandler {
 public:
  WebUIMessageHandler();
  virtual ~WebUIMessageHandler();

  // Attaches |this| to |web_ui| in order to handle messages from it.  Declared
  // virtual so that subclasses can do special init work as soon as the web_ui
  // is provided.  Returns |this| for convenience.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);

 protected:
  // Adds "url" and "title" keys on incoming dictionary, setting title
  // as the url as a fallback on empty title.
  static void SetURLAndTitle(base::DictionaryValue* dictionary,
                             string16 title,
                             const GURL& gurl);

  // This is where subclasses specify which messages they'd like to handle.
  virtual void RegisterMessages() = 0;

  // Extract an integer value from a list Value.
  bool ExtractIntegerValue(const base::ListValue* value, int* out_int);

  // Extract a floating point (double) value from a list Value.
  bool ExtractDoubleValue(const base::ListValue* value, double* out_value);

  // Extract a string value from a list Value.
  string16 ExtractStringValue(const base::ListValue* value);

  // Returns the attached WebUI for this handler.
  WebUI* web_ui() const { return web_ui_; }

  WebUI* web_ui_;  // TODO(wyck): Make private after merge conflicts go away.

 private:
  DISALLOW_COPY_AND_ASSIGN(WebUIMessageHandler);
};

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_H_
