// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_SCREENSAVER_EXTENSION_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_UI_SCREENSAVER_EXTENSION_DIALOG_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/extensions/extension_dialog_observer.h"

class Extension;
class ExtensionDialog;

namespace browser {

void ShowScreensaverDialog();
void CloseScreensaverDialog();

}  // namespace browser

// Shows or hides the screensaver extension in fullscreen mode on
// top of all other windows.
class ScreensaverExtensionDialog
    : public ExtensionDialogObserver {
 public:
  static void ShowScreensaverDialog();
  static void CloseScreensaverDialog();

  // ExtensionDialog::Observer implementation.
  virtual void ExtensionDialogClosing(ExtensionDialog* dialog) OVERRIDE;
  virtual void ExtensionTerminated(ExtensionDialog* dialog) OVERRIDE;

 protected:
  // ScreensaverDialog implementation.
  virtual void Show() OVERRIDE;
  virtual void Close() OVERRIDE;

 private:
  friend class ScreensaverExtensionDialogBrowserTest;
  friend class ScreensaverExtensionDialogTest;

  ScreensaverExtensionDialog();
  virtual ~ScreensaverExtensionDialog();

  // Load the screensaver extension.
  void LoadExtension();
  // Callback to set the extension and show the dialog.
  // Called from LoadExtension.
  void SetExtensionAndShow(scoped_refptr<Extension> extension);
  // Reload the screensaver extension and show another screensaver dialog.
  void ReloadAndShow();

  scoped_refptr<Extension> screensaver_extension_;
  // Host for the extension that implements this dialog.
  scoped_refptr<ExtensionDialog> extension_dialog_;

  // Set while we're loading an extension; only touched from the UI thread.
  bool loading_extension_;

  DISALLOW_COPY_AND_ASSIGN(ScreensaverExtensionDialog);
};

#endif  // CHROME_BROWSER_CHROMEOS_UI_SCREENSAVER_EXTENSION_DIALOG_H_
