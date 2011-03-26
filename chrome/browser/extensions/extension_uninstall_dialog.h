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
    virtual void ExtensionDialogAccepted() = 0;

    // We call this method to signal that the uninstallation should stop.
    virtual void ExtensionDialogCanceled() = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit ExtensionUninstallDialog(Profile* profile);
  virtual ~ExtensionUninstallDialog();

  // This is called by the extensions management page to verify whether the
  // uninstallation should proceed.
  // Starts the process of showing a confirmation UI, which is split into two.
  // 1) Set off a 'load icon' task.
  // 2) Handle the load icon response and show the UI (OnImageLoaded).
  void ConfirmUninstall(Delegate* delegate, const Extension* extension);

 private:
  // Creates an appropriate ExtensionUninstallDialog for the platform.
  static void Show(Profile* profile,
                   Delegate* delegate,
                   const Extension* extension,
                   SkBitmap* icon);

  // Sets the icon that will be used in any UI. If |icon| is NULL, or contains
  // an empty bitmap, then a default icon will be used instead.
  void SetIcon(SkBitmap* icon);

  // ImageLoadingTracker::Observer:
  virtual void OnImageLoaded(SkBitmap* image,
                             const ExtensionResource& resource,
                             int index) OVERRIDE;

  Profile* profile_;
  MessageLoop* ui_loop_;

  // The delegate we will call Accepted/Canceled on after confirmation UI.
  Delegate* delegate_;

  // The extension we are showing the UI for.
  const Extension* extension_;

  // Keeps track of extension images being loaded on the File thread for the
  // purpose of showing the install UI.
  ImageLoadingTracker tracker_;

  // The extensions icon.
  SkBitmap icon_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallDialog);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UNINSTALL_DIALOG_H_
