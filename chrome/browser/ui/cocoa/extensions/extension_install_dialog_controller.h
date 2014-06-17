// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALL_DIALOG_CONTROLER_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALL_DIALOG_CONTROLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"

namespace content {
class PageNavigator;
class WebContents;
}

@class ExtensionInstallViewController;

// Displays an extension install prompt as a tab modal dialog.
class ExtensionInstallDialogController :
    public ExtensionInstallPrompt::Delegate,
    public ConstrainedWindowMacDelegate {
 public:
  ExtensionInstallDialogController(
      const ExtensionInstallPrompt::ShowParams& show_params,
      ExtensionInstallPrompt::Delegate* delegate,
      scoped_refptr<ExtensionInstallPrompt::Prompt> prompt);
  virtual ~ExtensionInstallDialogController();

  // ExtensionInstallPrompt::Delegate implementation.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // ConstrainedWindowMacDelegate implementation.
  virtual void OnConstrainedWindowClosed(
      ConstrainedWindowMac* window) OVERRIDE;

  ConstrainedWindowMac* constrained_window() const {
    return constrained_window_.get();
  }
  ExtensionInstallViewController* view_controller() const {
    return view_controller_;
  }

 private:
  ExtensionInstallPrompt::Delegate* delegate_;
  base::scoped_nsobject<ExtensionInstallViewController> view_controller_;
  scoped_ptr<ConstrainedWindowMac> constrained_window_;
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALL_DIALOG_CONTROLLER_H_
