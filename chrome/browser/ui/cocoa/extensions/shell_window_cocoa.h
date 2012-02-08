// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_SHELL_WINDOW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_SHELL_WINDOW_COCOA_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/extensions/shell_window.h"

class ExtensionHost;
class ShellWindowCocoa;

// A window controller for a minimal window to host a web app view. Passes
// Objective-C notifications to the C++ bridge.
@interface ShellWindowController : NSWindowController<NSWindowDelegate> {
 @private
  ShellWindowCocoa* shellWindow_; // Weak; owns self.
}

@property(assign, nonatomic) ShellWindowCocoa* shellWindow;

@end

// Cocoa bridge to ShellWindow.
class ShellWindowCocoa : public ShellWindow {
 public:
  explicit ShellWindowCocoa(ExtensionHost* host);

  // ShellWindow implementation.
  virtual void Close() OVERRIDE;

  // Called when the window is about to be closed.
  void WindowWillClose();

 private:
  virtual ~ShellWindowCocoa();

  scoped_nsobject<ShellWindowController> window_controller_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_SHELL_WINDOW_COCOA_H_
