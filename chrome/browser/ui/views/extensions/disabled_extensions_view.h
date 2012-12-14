// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_DISABLED_EXTENSIONS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_DISABLED_EXTENSIONS_VIEW_H_

#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "chrome/common/extensions/extension_set.h"

class Browser;

namespace views {
class Label;
class Link;
class NativeTextButton;
}

// This is the class that implements the UI for the bubble showing which
// extensions have been automatically disabled by the sideload-wipeout
// initiative.
class DisabledExtensionsView : public views::BubbleDelegateView,
                               public views::ButtonListener,
                               public views::LinkListener {
 public:
  DisabledExtensionsView(views::View* anchor_view,
                         Browser* browser,
                         const ExtensionSet* wiped_out);

  // Show the Disabled Extension bubble, if needed.
  static void MaybeShow(Browser* browser, views::View* anchor_view);

  // Instruct the bubble to appear, whenever ready.
  void ShowWhenReady();

 private:
  virtual ~DisabledExtensionsView();

  // Shows the bubble and updates the counter for how often it has been shown.
  void ShowBubble();

  // Dismiss and make sure the bubble is not shown again. This bubble is a
  // single-appearance bubble.
  void DismissBubble();

  // views::BubbleDelegateView overrides:
  virtual void Init() OVERRIDE;

  // views::View overrides:
  virtual void Paint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener implementation.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // views::View implementation.
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE;

  base::WeakPtrFactory<DisabledExtensionsView> weak_factory_;

  Browser* browser_;

  // The set of extensions that have been wiped out by sideload wipeout.
  scoped_ptr<const ExtensionSet> wiped_out_;

  // The headline, labels and buttons on the bubble.
  views::Label* headline_;
  views::Link* learn_more_;
  views::NativeTextButton* settings_button_;
  views::NativeTextButton* dismiss_button_;
  views::Label* recourse_;

  // Offset (in pixels) of the Learn More link relative to the top left corner
  // (in LTR coordinate systems) of the |recourse_| label. This is used to
  // position the Learn More link.
  gfx::Size link_offset_;

  DISALLOW_COPY_AND_ASSIGN(DisabledExtensionsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_DISABLED_EXTENSIONS_VIEW_H_
