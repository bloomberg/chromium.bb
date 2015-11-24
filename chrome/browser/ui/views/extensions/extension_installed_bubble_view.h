// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble.h"
#include "components/bubble/bubble_reference.h"
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
class ExtensionInstalledBubbleView : public views::BubbleDelegateView {
 public:
  ExtensionInstalledBubbleView(ExtensionInstalledBubble* bubble,
                               BubbleReference bubble_reference);
  ~ExtensionInstalledBubbleView() override;

  // Recalculate the anchor position for this bubble.
  void UpdateAnchorView();

  // views::BubbleDelegateView:
  void WindowClosing() override;
  gfx::Rect GetAnchorRect() const override;
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

 private:
  BubbleReference bubble_reference_;
  const extensions::Extension* extension_;
  Browser* browser_;
  ExtensionInstalledBubble::BubbleType type_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_
