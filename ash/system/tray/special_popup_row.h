// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SPECIAL_POPUP_ROW_H_
#define ASH_SYSTEM_TRAY_SPECIAL_POPUP_ROW_H_

#include "ui/gfx/size.h"
#include "ui/views/view.h"

namespace views {
class Label;
}

namespace ash {
class ThrobberView;
class TrayItemView;
class TrayPopupHeaderButton;
class ViewClickListener;

// The 'special' looking row in the uber-tray popups. This is usually the bottom
// row in the popups, and has a fixed height.
class SpecialPopupRow : public views::View {
 public:
  SpecialPopupRow();
  virtual ~SpecialPopupRow();

  void SetTextLabel(int string_id, ViewClickListener* listener);
  void SetContent(views::View* view);

  void AddButton(TrayPopupHeaderButton* button);
  void AddThrobber(ThrobberView* throbber);

  views::View* content() const { return content_; }

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual int GetHeightForWidth(int width) const OVERRIDE;
  virtual void Layout() OVERRIDE;

  views::View* content_;
  views::View* button_container_;
  views::Label* text_label_;

  DISALLOW_COPY_AND_ASSIGN(SpecialPopupRow);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SPECIAL_POPUP_ROW_H_
