// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_GTK_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/gtk/custom_button.h"

class Browser;
class SkBitmap;

namespace extensions {
class Extension;
}

// Provides feedback to the user upon successful installation of an
// extension. Depending on the type of extension, the BubbleGtk will
// point to:
//    OMNIBOX_KEYWORD-> The omnibox.
//    BROWSER_ACTION -> The browserAction icon in the toolbar.
//    PAGE_ACTION    -> A preview of the page action icon in the location
//                      bar which is shown while the BubbleGtk is shown.
//    GENERIC        -> The wrench menu. This case includes page actions that
//                      don't specify a default icon.
//
// ExtensionInstallBubble manages its own lifetime.
class ExtensionInstalledBubbleGtk
    : public ExtensionInstalledBubble::Delegate,
      public BubbleDelegateGtk {
 public:
  virtual ~ExtensionInstalledBubbleGtk();

  // Creates the ExtensionInstalledBubble and schedules it to be shown once
  // the extension has loaded. |extension| is the installed extension. |browser|
  // is the browser window which will host the bubble. |icon| is the install
  // icon of the extension.
  static void Show(const extensions::Extension* extension,
                   Browser* browser,
                   const SkBitmap& icon);

 private:
  ExtensionInstalledBubbleGtk(const extensions::Extension* extension,
                              Browser* browser,
                              const SkBitmap& icon);

  // Notified when the bubble gets destroyed so we can delete our instance.
  CHROMEGTK_CALLBACK_0(ExtensionInstalledBubbleGtk, void, OnDestroy);

  // ExtensionInstalledBubble::Delegate:
  virtual bool MaybeShowNow() OVERRIDE;

  // BubbleDelegateGtk:
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

  static void OnButtonClick(GtkWidget* button,
                            ExtensionInstalledBubbleGtk* toolbar);

  // Link button callbacks.
  CHROMEGTK_CALLBACK_0(ExtensionInstalledBubbleGtk, void, OnLinkClicked);

  // The 'x' that the user can press to hide the bubble shelf.
  scoped_ptr<CustomDrawButton> close_button_;

  ExtensionInstalledBubble bubble_;
  BubbleGtk* gtk_bubble_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_GTK_H_
