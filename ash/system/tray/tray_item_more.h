// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_ITEM_MORE_H_
#define ASH_SYSTEM_TRAY_TRAY_ITEM_MORE_H_
#pragma once

#include "ash/system/tray/tray_views.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
class View;
}

namespace ash {

class SystemTrayItem;

namespace internal {

// A view with a chevron ('>') on the right edge. Clicking on the view brings up
// the detailed view of the tray-item that owns it.
class TrayItemMore : public ActionableView {
 public:
  explicit TrayItemMore(SystemTrayItem* owner);
  virtual ~TrayItemMore();

  void SetLabel(const string16& label);
  void SetImage(const gfx::ImageSkia* image_skia);
  void SetAccessibleName(const string16& name);

 protected:
  // Replaces the default icon (on the left of the label), and allows a custom
  // view to be palced there. Once the default icon is replaced, |SetImage|
  // should never be called.
  void ReplaceIcon(views::View* view);

 private:
  // Overridden from ActionableView.
  virtual bool PerformAction(const views::Event& event) OVERRIDE;

  // Overridden from views::View.
  virtual void Layout() OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  SystemTrayItem* owner_;
  views::ImageView* icon_;
  views::Label* label_;
  views::ImageView* more_;
  string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(TrayItemMore);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_ITEM_MORE_H_
