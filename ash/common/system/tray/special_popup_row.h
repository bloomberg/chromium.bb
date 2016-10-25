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

namespace views {
class Button;
class ButtonListener;
class CustomButton;
class Label;
class ToggleButton;
}

namespace ash {
class SystemMenuButton;
class ThrobberView;
class TrayItemView;
class TrayPopupHeaderButton;
class ViewClickListener;

// For material design, this class represents the top title row for detailed
// views and handles the creation and layout of its elements (back button,
// title, settings button, and possibly other buttons). For non-MD, this class
// represents the bottom row of detailed views and the bottom row of the
// system menu (date, help, power, and lock). This row has a fixed height.
class ASH_EXPORT SpecialPopupRow : public views::View {
 public:
  SpecialPopupRow();
  ~SpecialPopupRow() override;

  // Creates a text label corresponding to |string_id| and sets it as the
  // content of this row.
  void SetTextLabel(int string_id, ViewClickListener* listener);

  // Sets |content_| to be |view| and adds |content_| as a child view of this
  // row. This should only be called once, upon initialization of the row.
  // TODO(tdanderson): Make this private when material design is enabled by
  // default. See crbug.com/614453.
  void SetContent(views::View* view);

  // Creates UI elements for the material design title row and adds them to
  // the view hierarchy rooted at |this|. Returns a pointer to the created
  // view.
  views::Button* AddBackButton(views::ButtonListener* listener);
  views::CustomButton* AddSettingsButton(views::ButtonListener* listener,
                                         LoginStatus status);
  views::CustomButton* AddHelpButton(views::ButtonListener* listener,
                                     LoginStatus status);
  views::ToggleButton* AddToggleButton(views::ButtonListener* listener);

  // Adds |view| after this row's content.
  void AddViewToTitleRow(views::View* view);

  // Adds |view| after this row's content, optionally with a separator. Only
  // used for non-MD.
  // TODO(tdanderson): Remove this when material design is enabled by default.
  // See crbug.com/614453.
  void AddViewToRowNonMd(views::View* view, bool add_separator);

  // TODO(tdanderson): Remove this accessor when material design is enabled by
  // default. See crbug.com/614453.
  views::View* content() const { return content_; }

 private:
  // views::View:
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void Layout() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

  // Updates the style of |label_|, if it exists. Only used in material design.
  void UpdateStyle();

  // Used to add views to |views_before_content_container_| and
  // |views_after_content_container_|, respectively. Views are added to both
  // containers in a left-to-right order.
  void AddViewBeforeContent(views::View* view);
  void AddViewAfterContent(views::View* view);
  void AddViewAfterContent(views::View* view, bool add_separator);

  void SetTextLabelMd(int string_id, ViewClickListener* listener);
  void SetTextLabelNonMd(int string_id, ViewClickListener* listener);

  // The container for the views positioned before |content_|.
  views::View* views_before_content_container_;

  // The main content of this row, typically a label.
  views::View* content_;

  // The container for the views positioned after |content_|.
  views::View* views_after_content_container_;

  // A pointer to the label which is parented to |content_|; this is non-null
  // only if this row's content is a single label. Not owned.
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(SpecialPopupRow);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_SPECIAL_POPUP_ROW_H_
