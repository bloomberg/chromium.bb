// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_IMAGE_ITEM_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_IMAGE_ITEM_H_

#include "ash/ash_export.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "base/macros.h"

namespace views {
class ImageView;
}

namespace ash {
class TrayItemView;

class ASH_EXPORT TrayImageItem : public SystemTrayItem {
 public:
  TrayImageItem(SystemTray* system_tray, int resource_id);
  ~TrayImageItem() override;

  views::View* tray_view();

 protected:
  virtual bool GetInitialVisibility() = 0;

  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(LoginStatus status) override;
  views::View* CreateDefaultView(LoginStatus status) override;
  views::View* CreateDetailedView(LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void UpdateAfterLoginStatusChange(LoginStatus status) override;
  void UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) override;

  // Changes the icon of the tray-view to the specified resource.
  void SetImageFromResourceId(int resource_id);

 private:
  // Set the alignment of the image depending on the shelf alignment.
  void SetItemAlignment(ShelfAlignment alignment);

  int resource_id_;
  TrayItemView* tray_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayImageItem);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_IMAGE_ITEM_H_
