// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_WEB_AUTH_FLOW_WINDOW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_WEB_AUTH_FLOW_WINDOW_COCOA_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/extensions/web_auth_flow_window.h"

namespace content {
class BrowserContext;
class WebContents;
}

@class WebAuthFlowWindowController;

// Cocoa bridge to WebAuthFlowWindow.
class WebAuthFlowWindowCocoa : public WebAuthFlowWindow {
 public:
  WebAuthFlowWindowCocoa(
      Delegate* delegate,
      content::BrowserContext* browser_context,
      content::WebContents* contents);

  // WebAuthFlowWindow implementation.
  virtual void Show() OVERRIDE;

  // Called when the window is about to be closed.
  void WindowWillClose();

 private:
  virtual ~WebAuthFlowWindowCocoa();

  scoped_nsobject<WebAuthFlowWindowController> window_controller_;
  NSInteger attention_request_id_;  // identifier from requestUserAttention

  DISALLOW_COPY_AND_ASSIGN(WebAuthFlowWindowCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_WEB_AUTH_FLOW_WINDOW_COCOA_H_
