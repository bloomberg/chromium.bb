// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_DETAILS_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_DETAILS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/special_popup_row.h"
#include "ui/views/view.h"

namespace views {
class ScrollView;
}

namespace ash {
class FixedSizedScrollView;
class ScrollBorder;
class SystemTrayItem;
class ViewClickListener;

class ASH_EXPORT TrayDetailsView : public views::View {
 public:
  explicit TrayDetailsView(SystemTrayItem* owner);
  virtual ~TrayDetailsView();

  // Creates a row with special highlighting etc. This is typically the
  // bottom-most row in the popup.
  void CreateSpecialRow(int string_id, ViewClickListener* listener);

  // Creates a scrollable list. The list has a border at the bottom if there is
  // any other view between the list and the footer row at the bottom.
  void CreateScrollableList();

  // Adds a separator in scrollable list.
  void AddScrollSeparator();

  // Removes (and destroys) all child views.
  void Reset();

  // Transition to default view from details view. If |footer_| has focus before
  // transition, the default view should focus on the owner of this details
  // view.
  void TransitionToDefaultView();

  SystemTrayItem* owner() const { return owner_; }
  SpecialPopupRow* footer() const { return footer_; }
  FixedSizedScrollView* scroller() const { return scroller_; }
  views::View* scroll_content() const { return scroll_content_; }

 protected:
  // Overridden from views::View.
  virtual void Layout() OVERRIDE;
  virtual void OnPaintBorder(gfx::Canvas* canvas) OVERRIDE;

 private:
  SystemTrayItem* owner_;
  SpecialPopupRow* footer_;
  FixedSizedScrollView* scroller_;
  views::View* scroll_content_;
  ScrollBorder* scroll_border_;  // Weak reference

  DISALLOW_COPY_AND_ASSIGN(TrayDetailsView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_DETAILS_VIEW_H_
