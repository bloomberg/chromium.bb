// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
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
//    GENERIC        -> The app menu. This case includes pageActions that don't
//                      specify a default icon.
class ExtensionInstalledBubbleView
    : public ExtensionInstalledBubble::ExtensionInstalledBubbleUi,
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
  explicit ExtensionInstalledBubbleView(
      scoped_ptr<ExtensionInstalledBubble> bubble);

  ~ExtensionInstalledBubbleView() override;

  // ExtensionInstalledBubble::ExtensionInstalledBubbleUi:
  void Show() override;

  // views::WidgetDelegate:
  void WindowClosing() override;

  // views::BubbleDelegate:
  gfx::Rect GetAnchorRect() const override;

  scoped_ptr<ExtensionInstalledBubble> bubble_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_
