// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_DETAILS_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_DETAILS_VIEW_H_

#include "ui/views/view.h"

namespace views {
class ScrollView;
}

namespace ash {
namespace internal {

class FixedSizedScrollView;
class SpecialPopupRow;
class ViewClickListener;

class TrayDetailsView : public views::View {
 public:
  TrayDetailsView();
  virtual ~TrayDetailsView();

  // Creates a row with special highlighting etc. This is typically the
  // bottom-most row in the popup.
  void CreateSpecialRow(int string_id, ViewClickListener* listener);

  // Creates a scrollable list.
  void CreateScrollableList();

  // Removes (and destroys) all child views.
  void Reset();

  SpecialPopupRow* footer() const { return footer_; }
  FixedSizedScrollView* scroller() const { return scroller_; }
  views::View* scroll_content() const { return scroll_content_; }

 protected:
  // Overridden from views::View.
  virtual void Layout() OVERRIDE;

 private:
  SpecialPopupRow* footer_;
  FixedSizedScrollView* scroller_;
  views::View* scroll_content_;

  DISALLOW_COPY_AND_ASSIGN(TrayDetailsView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_DETAILS_VIEW_H_
