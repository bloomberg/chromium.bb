// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_DESKTOP_UI_FIND_BAR_VIEW_H_
#define MANDOLINE_UI_DESKTOP_UI_FIND_BAR_VIEW_H_

#include "base/macros.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
class Label;
class Textfield;
}

namespace mandoline {

class FindBarDelegate;

// Owns the widgets which show the find bar.
class FindBarView : public views::View,
                    public views::TextfieldController,
                    public views::ButtonListener {
 public:
  FindBarView(FindBarDelegate* delegate);
  ~FindBarView() override;

  void Show();
  void Hide();

  void SetMatchLabel(int result, int total);

 private:
  // Overridden from views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  FindBarDelegate* delegate_;

  views::BoxLayout* layout_;
  views::Textfield* text_field_;
  views::Label* match_count_label_;
  views::LabelButton* next_button_;
  views::LabelButton* prev_button_;
  views::LabelButton* close_button_;

  std::string last_find_string_;

  DISALLOW_COPY_AND_ASSIGN(FindBarView);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_DESKTOP_UI_FIND_BAR_VIEW_H_
