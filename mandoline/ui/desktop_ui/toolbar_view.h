// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_DESKTOP_UI_TOOLBAR_VIEW_H_
#define MANDOLINE_UI_DESKTOP_UI_TOOLBAR_VIEW_H_

#include "base/macros.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
class LabelButton;
}

namespace mandoline {
class BrowserWindow;

class ToolbarView : public views::View,
                    public views::ButtonListener {
 public:
  ToolbarView(BrowserWindow* browser_window);
  ~ToolbarView() override;

  // Sets the state of the visual elements.
  void SetOmniboxText(const base::string16& text);
  void SetBackForwardEnabled(bool back_enabled, bool forward_enabled);

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  BrowserWindow* browser_window_;

  views::BoxLayout* layout_;
  views::LabelButton* back_button_;
  views::LabelButton* forward_button_;
  views::LabelButton* omnibox_launcher_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarView);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_DESKTOP_UI_TOOLBAR_VIEW_H_
