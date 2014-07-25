// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_UNINSTALL_DIALOG_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_UNINSTALL_DIALOG_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace base {
class MessageLoop;
}

namespace gfx {
class Image;
}

namespace extensions {
class Extension;

class ExtensionUninstallDialog
    : public base::SupportsWeakPtr<ExtensionUninstallDialog> {
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

  // Creates a platform specific implementation of ExtensionUninstallDialog. The
  // dialog will be modal to |parent|, or a non-modal dialog if |parent| is
  // NULL.
  static ExtensionUninstallDialog* Create(Profile* profile,
                                          gfx::NativeWindow parent,
                                          Delegate* delegate);

  virtual ~ExtensionUninstallDialog();

  // This is called to verify whether the uninstallation should proceed.
  // Starts the process of showing a confirmation UI, which is split into two.
  // 1) Set off a 'load icon' task.
  // 2) Handle the load icon response and show the UI (OnImageLoaded).
  void ConfirmUninstall(const Extension* extension);

  // This shows the same dialog as above, except it also shows which extension
  // triggered the dialog by calling chrome.management.uninstall API.
  void ConfirmProgrammaticUninstall(const Extension* extension,
                                    const Extension* triggering_extension);

  std::string GetHeadingText();

 protected:
  // Constructor used by the derived classes.
  ExtensionUninstallDialog(Profile* profile,
                           gfx::NativeWindow parent,
                           Delegate* delegate);

  // TODO(sashab): Remove protected members: crbug.com/397395
  Profile* const profile_;

  // TODO(sashab): Investigate lifetime issue of this window variable:
  // crbug.com/397396
  gfx::NativeWindow parent_;

  // The delegate we will call Accepted/Canceled on after confirmation dialog.
  Delegate* delegate_;

  // The extension we are showing the dialog for.
  const Extension* extension_;

  // The extension triggering the dialog if the dialog was shown by
  // chrome.management.uninstall.
  const Extension* triggering_extension_;

  // The extensions icon.
  gfx::ImageSkia icon_;

 private:
  // Sets the icon that will be used in the dialog. If |icon| contains an empty
  // image, then we use a default icon instead.
  void SetIcon(const gfx::Image& image);

  void OnImageLoaded(const std::string& extension_id, const gfx::Image& image);

  // Displays the prompt. This should only be called after loading the icon.
  // The implementations of this method are platform-specific.
  virtual void Show() = 0;

  base::MessageLoop* ui_loop_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallDialog);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UNINSTALL_DIALOG_H_
