// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_ITEM_MORE_H_
#define ASH_SYSTEM_TRAY_TRAY_ITEM_MORE_H_

#include "ash/system/tray/actionable_view.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
class View;
}

namespace ash {
class SystemTrayItem;

// A view with a chevron ('>') on the right edge. Clicking on the view brings up
// the detailed view of the tray-item that owns it.
class TrayItemMore : public ActionableView {
 public:
  TrayItemMore(SystemTrayItem* owner, bool show_more);
  ~TrayItemMore() override;

  SystemTrayItem* owner() const { return owner_; }

  void SetLabel(const base::string16& label);
  void SetImage(const gfx::ImageSkia* image_skia);
  void SetAccessibleName(const base::string16& name);

 protected:
  // Replaces the default icon (on the left of the label), and allows a custom
  // view to be placed there. Once the default icon is replaced, |SetImage|
  // should never be called.
  void ReplaceIcon(views::View* view);

 private:
  // Overridden from ActionableView.
  bool PerformAction(const ui::Event& event) override;

  // Overridden from views::View.
  void Layout() override;
  void GetAccessibleState(ui::AXViewState* state) override;

  SystemTrayItem* owner_;
  // True if |more_| should be shown.
  bool show_more_;
  views::ImageView* icon_;
  views::Label* label_;
  views::ImageView* more_;
  base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(TrayItemMore);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_ITEM_MORE_H_
