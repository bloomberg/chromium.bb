// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_UNINSTALL_DIALOG_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_UNINSTALL_DIALOG_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/image/image_skia.h"

class Browser;
class Profile;

namespace base {
class MessageLoop;
}

namespace extensions {
class Extension;
}

namespace gfx {
class Image;
}

class ExtensionUninstallDialog
    : public content::NotificationObserver,
      public base::SupportsWeakPtr<ExtensionUninstallDialog> {
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
  // |profile| and |delegate| can never be NULL.
  // |browser| can be NULL only for Ash when this is used with the applist
  // window.
  static ExtensionUninstallDialog* Create(Profile* profile,
                                          Browser* browser,
                                          Delegate* delegate);

  virtual ~ExtensionUninstallDialog();

  // This is called to verify whether the uninstallation should proceed.
  // Starts the process of showing a confirmation UI, which is split into two.
  // 1) Set off a 'load icon' task.
  // 2) Handle the load icon response and show the UI (OnImageLoaded).
  void ConfirmUninstall(const extensions::Extension* extension);

 protected:
  // Constructor used by the derived classes.
  ExtensionUninstallDialog(Profile* profile,
                           Browser* browser,
                           Delegate* delegate);

#if defined(ENABLE_MANAGED_USERS)
  // Requests authorization from a managed user's custodian if required.
  bool ShowAuthorizationDialog();

  // If custodian authorization is granted, performs the uninstall, otherwise
  // cancels uninstall.
  void OnAuthorizationResult(bool success);
#endif

  Profile* const profile_;

  Browser* browser_;

  // The delegate we will call Accepted/Canceled on after confirmation dialog.
  Delegate* delegate_;

  // The extension we are showing the dialog for.
  const extensions::Extension* extension_;

  // The extensions icon.
  gfx::ImageSkia icon_;

 private:
  // Sets the icon that will be used in the dialog. If |icon| contains an empty
  // image, then we use a default icon instead.
  void SetIcon(const gfx::Image& image);

  void OnImageLoaded(const gfx::Image& image);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Displays the prompt. This should only be called after loading the icon.
  // The implementations of this method are platform-specific.
  virtual void Show() = 0;

  // Keeps track of whether we're still waiting for an image to load before
  // we show the dialog.
  enum State {
    kImageIsLoading,  // Image is loading asynchronously.
    kDialogIsShowing, // Dialog is shown after image is loaded.
    kBrowserIsClosing // Browser is closed while image is still loading.
  };
  State state_;

  base::MessageLoop* ui_loop_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallDialog);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UNINSTALL_DIALOG_H_
