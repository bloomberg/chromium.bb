// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble.h"
#include "ui/views/bubble/bubble_delegate.h"

class Browser;

namespace extensions {
class Extension;
}

// Provides feedback to the user upon successful installation of an
// extension. Depending on the type of extension, the Bubble will
// point to:
//    OMNIBOX_KEYWORD-> The omnibox.
//    BROWSER_ACTION -> The browserAction icon in the toolbar.
//    PAGE_ACTION    -> A preview of the pageAction icon in the location
//                      bar which is shown while the Bubble is shown.
//    GENERIC        -> The wrench menu. This case includes pageActions that
//                      don't specify a default icon.
class ExtensionInstalledBubbleView
    : public ExtensionInstalledBubble::Delegate,
      public views::BubbleDelegateView {
 public:
  // Creates the ExtensionInstalledBubbleView and schedules it to be shown once
  // the extension has loaded. |extension| is the installed extension. |browser|
  // is the browser window which will host the bubble. |icon| is the install
  // icon of the extension.
  static void Show(const extensions::Extension* extension,
                   Browser* browser,
                   const SkBitmap& icon);

 private:
  ExtensionInstalledBubbleView(const extensions::Extension* extension,
                               Browser* browser,
                               const SkBitmap& icon);

  virtual ~ExtensionInstalledBubbleView();

  // ExtensionInstalledBubble::Delegate:
  virtual bool MaybeShowNow() OVERRIDE;

  // views::WidgetDelegate:
  virtual void WindowClosing() OVERRIDE;

  // views::BubbleDelegate:
  virtual gfx::Rect GetAnchorRect() OVERRIDE;

  ExtensionInstalledBubble bubble_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_
