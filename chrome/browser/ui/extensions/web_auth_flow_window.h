// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_WEB_AUTH_FLOW_WINDOW_H_
#define CHROME_BROWSER_UI_EXTENSIONS_WEB_AUTH_FLOW_WINDOW_H_
#pragma once

#include "base/basictypes.h"

class Profile;

namespace content {
class BrowserContext;
class WebContents;
}

// Platform independent abstraction for a window that performs web auth flow.
// Platform specific implementations implement this abstract class.
class WebAuthFlowWindow {
 public:
  class Delegate {
   public:
    virtual void OnClose() = 0;
  };

  // TODO(munjal): Allow customizing these?
  static const int kDefaultWidth = 1024;
  static const int kDefaultHeight = 768;

  // Creates a new instance of WebAuthFlowWindow with the given parmaters.
  // Delegate::OnClose will be called when the window is closed.
  static WebAuthFlowWindow* Create(Delegate* delegate,
                                   content::BrowserContext* browser_context,
                                   content::WebContents* contents);

  virtual ~WebAuthFlowWindow();

  // Show the window.
  virtual void Show() = 0;

 protected:
  WebAuthFlowWindow(Delegate* delegate,
                    content::BrowserContext* browser_context,
                    content::WebContents* contents);

  Delegate* delegate() { return delegate_; }
  content::BrowserContext* browser_context() { return browser_context_; }
  content::WebContents* contents() { return contents_; }

 private:
  Delegate* delegate_;
  content::BrowserContext* browser_context_;
  content::WebContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthFlowWindow);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_WEB_AUTH_FLOW_WINDOW_H_
