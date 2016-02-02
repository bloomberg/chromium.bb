// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "components/bubble/bubble_reference.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

class Browser;
class BubbleSyncPromoView;

namespace extensions {
class Extension;
}

namespace views {
class LabelButton;
class Link;
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
class ExtensionInstalledBubbleView : public BubbleSyncPromoDelegate,
                                     public views::BubbleDelegateView,
                                     public views::ButtonListener,
                                     public views::LinkListener {
 public:
  ExtensionInstalledBubbleView(ExtensionInstalledBubble* bubble,
                               BubbleReference bubble_reference);
  ~ExtensionInstalledBubbleView() override;

  // Recalculate the anchor position for this bubble.
  void UpdateAnchorView();

  void InitLayout();

 private:
  // views::BubbleDelegateView:
  scoped_ptr<views::View> CreateFootnoteView() override;
  void WindowClosing() override;
  gfx::Rect GetAnchorRect() const override;
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // BubbleSyncPromoDelegate:
  void OnSignInLinkClicked() override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  const ExtensionInstalledBubble* bubble_;
  BubbleReference bubble_reference_;
  const extensions::Extension* extension_;
  Browser* browser_;
  ExtensionInstalledBubble::BubbleType type_;
  ExtensionInstalledBubble::AnchorPosition anchor_position_;

  // The button to close the bubble.
  views::LabelButton* close_;

  // The shortcut to open the manage shortcuts page.
  views::Link* manage_shortcut_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_VIEW_H_
