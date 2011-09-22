// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_UNINSTALL_DIALOG_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_UNINSTALL_DIALOG_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "third_party/skia/include/core/SkBitmap.h"

class MessageLoop;
class Profile;

class ExtensionUninstallDialog : public ImageLoadingTracker::Observer {
 public:
  class Delegate {
   public:
    // We call this method to signal that the uninstallation should continue.
    virtual void ExtensionUninstallAccepted() = 0;

    // We call this method to signal that the uninstallation should stop.
    virtual void ExtensionUninstallCanceled() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Creates a platform specific implementation of ExtensionUninstallDialog.
  static ExtensionUninstallDialog* Create(
      Profile* profile, Delegate* delegate);

  virtual ~ExtensionUninstallDialog();

  // This is called to verify whether the uninstallation should proceed.
  // Starts the process of showing a confirmation UI, which is split into two.
  // 1) Set off a 'load icon' task.
  // 2) Handle the load icon response and show the UI (OnImageLoaded).
  void ConfirmUninstall(const Extension* extension);

 protected:
  // Constructor used by the derived classes.
  explicit ExtensionUninstallDialog(Profile* profile, Delegate* delegate);

  Profile* profile_;

  // The delegate we will call Accepted/Canceled on after confirmation dialog.
  Delegate* delegate_;

  // The extension we are showing the dialog for.
  const Extension* extension_;

  // The extensions icon.
  SkBitmap icon_;

 private:
  // Sets the icon that will be used in the dialog. If |icon| is NULL, or
  // contains an empty bitmap, then we use a default icon instead.
  void SetIcon(SkBitmap* icon);

  // ImageLoadingTracker::Observer:
  virtual void OnImageLoaded(SkBitmap* image,
                             const ExtensionResource& resource,
                             int index) OVERRIDE;

  // Displays the prompt. This should only be called after loading the icon.
  // The implementations of this method are platform-specific.
  virtual void Show() = 0;

  MessageLoop* ui_loop_;

  // Keeps track of extension images being loaded on the File thread for the
  // purpose of showing the dialog.
  ImageLoadingTracker tracker_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallDialog);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UNINSTALL_DIALOG_H_
