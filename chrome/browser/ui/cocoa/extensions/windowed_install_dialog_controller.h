// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_WINDOWED_INSTALL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_WINDOWED_INSTALL_DIALOG_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/gtest_prod_util.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/extensions/extension_install_prompt.h"

@class ExtensionInstallViewController;
@class WindowedInstallController;

// Displays an app or extension install or permissions prompt as a standalone
// NSPanel.
class WindowedInstallDialogController
    : public ExtensionInstallPrompt::Delegate {
 public:
  // Initializes the ExtensionInstallViewController and shows the window. This
  // object will delete itself when the window is closed.
  WindowedInstallDialogController(
      const ExtensionInstallPrompt::ShowParams& show_params,
      ExtensionInstallPrompt::Delegate* delegate,
      scoped_refptr<ExtensionInstallPrompt::Prompt> prompt);
  virtual ~WindowedInstallDialogController();

  // Invoked by the -[NSWindow windowWillClose:] notification after a dialog
  // choice is invoked. Releases owned resources, then deletes |this|.
  void OnWindowClosing();

  // ExtensionInstallPrompt::Delegate:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(WindowedInstallDialogControllerBrowserTest,
                           ShowInstallDialog);
  ExtensionInstallViewController* GetViewController();

  ExtensionInstallPrompt::Delegate* delegate_;
  base::scoped_nsobject<WindowedInstallController> install_controller_;

  DISALLOW_COPY_AND_ASSIGN(WindowedInstallDialogController);
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_WINDOWED_INSTALL_DIALOG_CONTROLLER_H_
