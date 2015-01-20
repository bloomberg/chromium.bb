// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/supports_user_data.h"
#include "ios/web/public/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class GURL;

#if defined(__OBJC__)
// TODO(droger): Convert all the public web API to C++.
@class CRWJSInjectionReceiver;
#else
class CRWJSInjectionReceiver;
#endif  // defined(__OBJC__)

namespace base {
class DictionaryValue;
}

namespace web {

class BrowserState;
class NavigationManager;
class WebStateObserver;

// Core interface for interaction with the web.
class WebState : public base::SupportsUserData {
 public:
  // Parameters for the OpenURL() method.
  struct OpenURLParams {
    OpenURLParams(const GURL& url,
                  const Referrer& referrer,
                  WindowOpenDisposition disposition,
                  ui::PageTransition transition,
                  bool is_renderer_initiated);
    ~OpenURLParams();

    // The URL/referrer to be opened.
    GURL url;
    Referrer referrer;

    // The disposition requested by the navigation source.
    WindowOpenDisposition disposition;

    // The transition type of navigation.
    ui::PageTransition transition;

    // Whether this navigation is initiated by the renderer process.
    bool is_renderer_initiated;
  };

  ~WebState() override {}

  // Gets the BrowserState associated with this WebState. Can never return null.
  virtual BrowserState* GetBrowserState() const = 0;

  // Opens a URL with the given disposition.  The transition specifies how this
  // navigation should be recorded in the history system (for example, typed).
  virtual void OpenURL(const OpenURLParams& params) = 0;

  // Gets the NavigationManager associated with this WebState. Can never return
  // null.
  virtual NavigationManager* GetNavigationManager() = 0;

  // Gets the CRWJSInjectionReceiver associated with this WebState.
  virtual CRWJSInjectionReceiver* GetJSInjectionReceiver() const = 0;

  // Gets the contents MIME type.
  virtual const std::string& GetContentsMimeType() const = 0;

  // Gets the value of the "Content-Language" HTTP header.
  virtual const std::string& GetContentLanguageHeader() const = 0;

  // Returns true if the current view is a web view with HTML.
  virtual bool ContentIsHTML() const = 0;

  // Gets the URL currently being displayed in the URL bar, if there is one.
  // This URL might be a pending navigation that hasn't committed yet, so it is
  // not guaranteed to match the current page in this WebState. A typical
  // example of this is interstitials, which show the URL of the new/loading
  // page (active) but the security context is of the old page (last committed).
  virtual const GURL& GetVisibleURL() const = 0;

  // Gets the last committed URL. It represents the current page that is
  // displayed in this WebState. It represents the current security context.
  virtual const GURL& GetLastCommittedURL() const = 0;

  // Callback used to handle script commands.
  // The callback must return true if the command was handled, and false
  // otherwise.
  // In particular the callback must return false if the command is unexpected
  // or ill-formatted.
  // The first parameter is the content of the command, the second parameter is
  // the URL of the page, and the third parameter is a bool indicating if the
  // user is currently interacting with the page.
  typedef base::Callback<bool(const base::DictionaryValue&, const GURL&, bool)>
      ScriptCommandCallback;

  // Registers a callback that will be called when a command matching
  // |command_prefix| is received.
  virtual void AddScriptCommandCallback(const ScriptCommandCallback& callback,
                                        const std::string& command_prefix) = 0;

  // Removes the callback associated with |command_prefix|.
  virtual void RemoveScriptCommandCallback(
      const std::string& command_prefix) = 0;

 protected:
  friend class WebStateObserver;

  // Adds and removes observers for page navigation notifications. The order in
  // which notifications are sent to observers is undefined. Clients must be
  // sure to remove the observer before they go away.
  // TODO(droger): Move these methods to WebStateImpl once it is in ios/.
  virtual void AddObserver(WebStateObserver* observer) = 0;
  virtual void RemoveObserver(WebStateObserver* observer) = 0;

  WebState() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_H_
