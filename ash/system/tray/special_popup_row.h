// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SPECIAL_POPUP_ROW_H_
#define ASH_SYSTEM_TRAY_SPECIAL_POPUP_ROW_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/size.h"
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
class ASH_EXPORT SpecialPopupRow : public views::View {
 public:
  SpecialPopupRow();
  ~SpecialPopupRow() override;

  void SetTextLabel(int string_id, ViewClickListener* listener);
  void SetContent(views::View* view);

  void AddView(views::View* view, bool add_separator);
  void AddButton(TrayPopupHeaderButton* button);

  views::View* content() const { return content_; }

 private:
  // Overridden from views::View.
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void Layout() override;

  views::View* content_;
  views::View* button_container_;

  DISALLOW_COPY_AND_ASSIGN(SpecialPopupRow);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SPECIAL_POPUP_ROW_H_
