// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_DOM_UI_H_
#define CHROME_BROWSER_DOM_UI_DOM_UI_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/string16.h"
#include "chrome/common/page_transition_types.h"

class DictionaryValue;
class DOMMessageHandler;
class GURL;
class ListValue;
class Profile;
class RenderViewHost;
class TabContents;
class Value;
struct ViewHostMsg_DomMessage_Params;

namespace ui {
class ThemeProvider;
}

// A DOMUI sets up the datasources and message handlers for a given HTML-based
// UI. It is contained by a DOMUIManager.
class DOMUI {
 public:
  explicit DOMUI(TabContents* contents);
  virtual ~DOMUI();

  // Called by RenderViewHost when the RenderView is first created. This is
  // *not* called for every page load because in some cases
  // RenderViewHostManager will reuse RenderView instances. In those cases,
  // RenderViewReused will be called instead.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) {}

  // Called by RenderViewHostManager when a RenderView is reused to display a
  // page.
  virtual void RenderViewReused(RenderViewHost* render_view_host) {}

  // Called when this becomes the active DOMUI instance for a re-used
  // RenderView; this is the point at which this DOMUI instance will receive
  // DOM messages instead of the previous DOMUI instance.
  //
  // If a DOMUI instance has code that is usually triggered from a JavaScript
  // onload handler, this should be overridden to check to see if the web page's
  // DOM is still intact (e.g., due to a back/forward navigation that remains
  // within the same page), and if so trigger that code manually since onload
  // won't be run in that case.
  virtual void DidBecomeActiveForReusedRenderView() {}

  // Called from TabContents.
  virtual void ProcessDOMUIMessage(const ViewHostMsg_DomMessage_Params& params);

  // Used by DOMMessageHandlers.
  typedef Callback1<const ListValue*>::Type MessageCallback;
  void RegisterMessageCallback(const std::string& message,
                               MessageCallback* callback);

  // Returns true if the favicon should be hidden for the current tab.
  bool hide_favicon() const {
    return hide_favicon_;
  }

  // Returns true if the bookmark bar should be forced to being visible,
  // overriding the user's preference.
  bool force_bookmark_bar_visible() const {
    return force_bookmark_bar_visible_;
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
  PageTransition::Type link_transition_type() const {
    return link_transition_type_;
  }

  int bindings() const {
    return bindings_;
  }

  // Call a Javascript function by sending its name and arguments down to
  // the renderer.  This is asynchronous; there's no way to get the result
  // of the call, and should be thought of more like sending a message to
  // the page.
  // There are variants for calls with more arguments.
  void CallJavascriptFunction(const std::wstring& function_name);
  void CallJavascriptFunction(const std::wstring& function_name,
                              const Value& arg);
  void CallJavascriptFunction(const std::wstring& function_name,
                              const Value& arg1,
                              const Value& arg2);
  void CallJavascriptFunction(const std::wstring& function_name,
                              const Value& arg1,
                              const Value& arg2,
                              const Value& arg3);
  void CallJavascriptFunction(const std::wstring& function_name,
                              const Value& arg1,
                              const Value& arg2,
                              const Value& arg3,
                              const Value& arg4);
  void CallJavascriptFunction(const std::wstring& function_name,
                              const std::vector<const Value*>& args);

  ui::ThemeProvider* GetThemeProvider() const;

  // May be overridden by DOMUI's which do not have a tab contents.
  virtual Profile* GetProfile() const;

  // May be overridden by DOMUI's which do not have a tab contents.
  virtual RenderViewHost* GetRenderViewHost() const;

  TabContents* tab_contents() const { return tab_contents_; }

 protected:
  void AddMessageHandler(DOMMessageHandler* handler);

  // Execute a string of raw Javascript on the page.  Overridable for
  // testing purposes.
  virtual void ExecuteJavascript(const std::wstring& javascript);

  // Options that may be overridden by individual Web UI implementations. The
  // bool options default to false. See the public getters for more information.
  bool hide_favicon_;
  bool force_bookmark_bar_visible_;
  bool focus_location_bar_by_default_;
  bool should_hide_url_;
  string16 overridden_title_;  // Defaults to empty string.
  PageTransition::Type link_transition_type_;  // Defaults to LINK.
  int bindings_;  // The bindings from BindingsPolicy that should be enabled for
                  // this page.

  // The DOMMessageHandlers we own.
  std::vector<DOMMessageHandler*> handlers_;

  // Non-owning pointer to the TabContents this DOMUI is associated with.
  TabContents* tab_contents_;

 private:
  // A map of message name -> message handling callback.
  typedef std::map<std::string, MessageCallback*> MessageCallbackMap;
  MessageCallbackMap message_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(DOMUI);
};

// Messages sent from the DOM are forwarded via the DOMUI to handler
// classes. These objects are owned by DOMUI and destroyed when the
// host is destroyed.
class DOMMessageHandler {
 public:
  DOMMessageHandler() : dom_ui_(NULL) {}
  virtual ~DOMMessageHandler() {}

  // Attaches |this| to |dom_ui| in order to handle messages from it.  Declared
  // virtual so that subclasses can do special init work as soon as the dom_ui
  // is provided.  Returns |this| for convenience.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);

 protected:
  // Adds "url" and "title" keys on incoming dictionary, setting title
  // as the url as a fallback on empty title.
  static void SetURLAndTitle(DictionaryValue* dictionary,
                             string16 title,
                             const GURL& gurl);

  // This is where subclasses specify which messages they'd like to handle.
  virtual void RegisterMessages() = 0;

  // Extract an integer value from a list Value.
  bool ExtractIntegerValue(const ListValue* value, int* out_int);

  // Extract a string value from a list Value.
  std::wstring ExtractStringValue(const ListValue* value);

  DOMUI* dom_ui_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DOMMessageHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_DOM_UI_H_
