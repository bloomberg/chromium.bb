// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_DETAILS_VIEW_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_DETAILS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/common/system/tray/special_popup_row.h"
#include "ash/common/system/tray/view_click_listener.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ScrollView;
class ProgressBar;
}  // namespace views

namespace ash {
namespace test {
class TrayDetailsViewTest;
}  // namespace test

class FixedSizedScrollView;
class ScrollBorder;
class SystemTrayItem;

class ASH_EXPORT TrayDetailsView : public views::View,
                                   public ViewClickListener,
                                   public views::ButtonListener {
 public:
  explicit TrayDetailsView(SystemTrayItem* owner);
  ~TrayDetailsView() override;

  // ViewClickListener:
  void OnViewClicked(views::View* sender) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  SystemTrayItem* owner() { return owner_; }
  SpecialPopupRow* title_row() { return title_row_; }
  FixedSizedScrollView* scroller() { return scroller_; }
  views::View* scroll_content() { return scroll_content_; }
  views::ProgressBar* progress_bar() { return progress_bar_; }

 protected:
  // views::View:
  void Layout() override;
  void OnPaintBorder(gfx::Canvas* canvas) override;

  // Creates the row containing the back button and title. For material design
  // this appears at the top of the view, for non-material design it appears
  // at the bottom.
  void CreateTitleRow(int string_id);

  // Creates a scrollable list. The list has a border at the bottom if there is
  // any other view between the list and the footer row at the bottom.
  void CreateScrollableList();

  // Creates a progress bar which overlaps with bottom edge of the |title_row_|.
  // |title_row_| needs to be created before this method is called.
  void CreateProgressBar();

  // Adds a separator in scrollable list.
  void AddScrollSeparator();

  // Removes (and destroys) all child views.
  void Reset();

 private:
  friend class test::TrayDetailsViewTest;

  // Overridden to handle clicks on subclass-specific views.
  virtual void HandleViewClicked(views::View* view);

  // Overridden to handle button presses on subclass-specific buttons.
  virtual void HandleButtonPressed(views::Button* sender,
                                   const ui::Event& event);

  // Creates and adds subclass-specific buttons to the title row.
  virtual void CreateExtraTitleRowButtons();

  // Transition to default view from details view. If |title_row_| has focus
  // before transition, the default view should focus on the owner of this
  // details view.
  void TransitionToDefaultView();

  SystemTrayItem* owner_;
  SpecialPopupRow* title_row_;
  FixedSizedScrollView* scroller_;
  views::View* scroll_content_;
  views::ProgressBar* progress_bar_;

  // |title_row_separator_| has a views::Separator as a child and, optionally,
  // |progress_bar_| as a child.
  views::View* title_row_separator_;

  ScrollBorder* scroll_border_;  // Weak reference

  // The back button that appears in the material design title row. Not owned.
  views::Button* back_button_;

  DISALLOW_COPY_AND_ASSIGN(TrayDetailsView);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_DETAILS_VIEW_H_
