// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_IMAGE_ITEM_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_IMAGE_ITEM_H_

#include "ash/ash_export.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"

namespace views {
class ImageView;
}

namespace ash {
class TrayItemView;

class ASH_EXPORT TrayImageItem : public SystemTrayItem {
 public:
  TrayImageItem(SystemTray* system_tray, int resource_id, UmaType uma_type);
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

  // Sets the color of the material design icon to |color|.
  void SetIconColor(SkColor color);

  // Changes the icon of the tray-view to the specified resource.
  // TODO(tdanderson): This is only used for non-material design, so remove it
  // when material design is the default. See crbug.com/625692.
  void SetImageFromResourceId(int resource_id);

 private:
  // Sets the current icon on |tray_view_|'s ImageView.
  void UpdateImageOnImageView();

  // The resource ID for the non-material design icon in the tray.
  // TODO(tdanderson): This is only used for non-material design, so remove it
  // when material design is the default. See crbug.com/625692.
  int resource_id_;

  // The color of the material design icon in the tray.
  SkColor icon_color_;

  TrayItemView* tray_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayImageItem);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_IMAGE_ITEM_H_
