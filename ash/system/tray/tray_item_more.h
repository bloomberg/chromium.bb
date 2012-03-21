// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_ITEM_MORE_H_
#define ASH_SYSTEM_TRAY_TRAY_ITEM_MORE_H_
#pragma once

#include "ui/views/view.h"

namespace views {
class ImageView;
}

namespace ash {

class SystemTrayItem;

namespace internal {

// A view with a chevron ('>') on the right edge. Clicking on the view brings up
// the detailed view of the tray-item that owns it.
class TrayItemMore : public views::View {
 public:
  explicit TrayItemMore(SystemTrayItem* owner);
  virtual ~TrayItemMore();

  void AddMore();

 private:
  // Overridden from views::View.
  virtual void Layout() OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;

  SystemTrayItem* owner_;
  views::ImageView* more_;

  DISALLOW_COPY_AND_ASSIGN(TrayItemMore);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_ITEM_MORE_H_
