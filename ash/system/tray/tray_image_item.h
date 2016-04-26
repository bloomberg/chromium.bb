// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_IMAGE_ITEM_H_
#define ASH_SYSTEM_TRAY_TRAY_IMAGE_ITEM_H_

#include "ash/system/tray/system_tray_item.h"
#include "base/macros.h"

namespace views {
class ImageView;
}

namespace ash {
class TrayItemView;

class TrayImageItem : public SystemTrayItem {
 public:
  TrayImageItem(SystemTray* system_tray, int resource_id);
  ~TrayImageItem() override;

  views::View* tray_view();

  // Changes the icon of the tray-view to the specified resource.
  void SetImageFromResourceId(int resource_id);

 protected:
  virtual bool GetInitialVisibility() = 0;

  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(user::LoginStatus status) override;
  views::View* CreateDefaultView(user::LoginStatus status) override;
  views::View* CreateDetailedView(user::LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void UpdateAfterLoginStatusChange(user::LoginStatus status) override;
  void UpdateAfterShelfAlignmentChange(wm::ShelfAlignment alignment) override;

 private:
  // Set the alignment of the image depending on the shelf alignment.
  void SetItemAlignment(wm::ShelfAlignment alignment);

  int resource_id_;
  TrayItemView* tray_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayImageItem);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_IMAGE_ITEM_H_
