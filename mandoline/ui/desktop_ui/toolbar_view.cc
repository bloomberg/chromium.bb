// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/desktop_ui/toolbar_view.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "mandoline/ui/desktop_ui/browser_window.h"
#include "ui/views/layout/box_layout.h"

namespace mandoline {

ToolbarView::ToolbarView(BrowserWindow* browser_window)
    : browser_window_(browser_window),
      layout_(new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 5)),
      back_button_(new views::LabelButton(this, base::ASCIIToUTF16("Back"))),
      forward_button_(
          new views::LabelButton(this, base::ASCIIToUTF16("Forward"))),
      omnibox_launcher_(
          new views::LabelButton(this, base::ASCIIToUTF16("Open Omnibox"))) {
  SetLayoutManager(layout_);

  AddChildView(back_button_);
  AddChildView(forward_button_);
  AddChildView(omnibox_launcher_);

  layout_->SetDefaultFlex(0);
  layout_->SetFlexForView(omnibox_launcher_, 1);
}

ToolbarView::~ToolbarView() {}

void ToolbarView::SetOmniboxText(const base::string16& text) {
  omnibox_launcher_->SetText(text);
}

void ToolbarView::SetBackForwardEnabled(bool back_enabled,
                                        bool forward_enabled) {
  back_button_->SetEnabled(back_enabled);
  forward_button_->SetEnabled(forward_enabled);
}

void ToolbarView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (sender == omnibox_launcher_)
    browser_window_->ShowOmnibox();
  else if (sender == back_button_)
    browser_window_->GoBack();
  else if (sender == forward_button_)
    browser_window_->GoForward();
  else
    NOTREACHED();
}

}  // namespace mandoline
