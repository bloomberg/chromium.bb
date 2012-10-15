// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALL_DIALOG_CONTROLER_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALL_DIALOG_CONTROLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_controller.h"

namespace content {
class PageNavigator;
class WebContents;
}

@class ExtensionInstallViewController;

// Displays an extension install prompt as a tab modal dialog.
class ExtensionInstallDialogController :
    public ExtensionInstallPrompt::Delegate {
 public:
  ExtensionInstallDialogController(
      content::WebContents* webContents,
      content::PageNavigator* navigator,
      ExtensionInstallPrompt::Delegate* delegate,
      const ExtensionInstallPrompt::Prompt& prompt);
  virtual ~ExtensionInstallDialogController();

  // ExtensionInstallPrompt::Delegate implementation.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  ExtensionInstallViewController* view_controller() const {
    return view_controller_;
  }

  ConstrainedWindowController* window_controller() const {
    return window_controller_;
  }

 private:
  ExtensionInstallPrompt::Delegate* const delegate_;
  scoped_nsobject<ExtensionInstallViewController> view_controller_;
  scoped_nsobject<ConstrainedWindowController> window_controller_;
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALL_DIALOG_CONTROLLER_H_
