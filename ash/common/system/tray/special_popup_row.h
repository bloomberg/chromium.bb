// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_SPECIAL_POPUP_ROW_H_
#define ASH_COMMON_SYSTEM_TRAY_SPECIAL_POPUP_ROW_H_

#include "ash/ash_export.h"
#include "ash/common/login_status.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace ash {
class ViewClickListener;

// Not used in material design. This class represents the bottom row of
// detailed views and the bottom row of the system menu (date, help, power,
// and lock). This row has a fixed height.
// TODO(tdanderson): Remove this class when material design is enabled by
// default. See crbug.com/614453.
class ASH_EXPORT SpecialPopupRow : public views::View {
 public:
  SpecialPopupRow();
  ~SpecialPopupRow() override;

  // Creates a text label corresponding to |string_id| and sets it as the
  // content of this row.
  void SetTextLabel(int string_id, ViewClickListener* listener);

  // Sets |content_| to be |view| and adds |content_| as a child view of this
  // row. This should only be called once, upon initialization of the row.
  void SetContent(views::View* view);

  // Adds |view| after this row's content.
  void AddViewToTitleRow(views::View* view);

  // Adds |view| after this row's content, optionally with a separator. Only
  // used for non-MD.
  void AddViewToRowNonMd(views::View* view, bool add_separator);

  views::View* content() const { return content_; }

 private:
  // views::View:
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void Layout() override;

  // Used to add views to |views_after_content_container_|, respectively. Views
  // are added in a left-to-right order.
  void AddViewAfterContent(views::View* view);
  void AddViewAfterContent(views::View* view, bool add_separator);

  // The main content of this row, typically a label.
  views::View* content_;

  // The container for the views positioned after |content_|.
  views::View* views_after_content_container_;

  DISALLOW_COPY_AND_ASSIGN(SpecialPopupRow);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_SPECIAL_POPUP_ROW_H_
