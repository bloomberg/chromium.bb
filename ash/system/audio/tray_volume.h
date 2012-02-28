// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_TRAY_VOLUME_H_
#define ASH_SYSTEM_AUDIO_TRAY_VOLUME_H_
#pragma once

#include "ash/system/tray/system_tray_item.h"
#include "base/memory/scoped_ptr.h"

namespace views {
class ImageView;
}

namespace tray {
class VolumeView;
}

namespace ash {
namespace internal {

class TrayVolume : public SystemTrayItem {
 public:
  TrayVolume();
  virtual ~TrayVolume();

 private:
  // Overridden from SystemTrayItem
  virtual views::View* CreateTrayView() OVERRIDE;
  virtual views::View* CreateDefaultView() OVERRIDE;
  virtual views::View* CreateDetailedView() OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;

  scoped_ptr<tray::VolumeView> volume_view_;
  scoped_ptr<views::ImageView> tray_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayVolume);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_TRAY_VOLUME_H_
