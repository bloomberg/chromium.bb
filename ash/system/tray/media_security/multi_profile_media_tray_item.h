// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_MULTI_PROFILE_MEDIA_TRAY_ITEM_H_
#define ASH_SYSTEM_CHROMEOS_MULTI_PROFILE_MEDIA_TRAY_ITEM_H_

#include "ash/system/tray/system_tray_item.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/views/view.h"

namespace ash {
namespace tray {
class MultiProfileMediaTrayView;
}

// The tray item for media recording.
class ASH_EXPORT MultiProfileMediaTrayItem : public SystemTrayItem {
 public:
  explicit MultiProfileMediaTrayItem(SystemTray* system_tray);
  virtual ~MultiProfileMediaTrayItem();

  // SystemTrayItem:
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;

 private:
  tray::MultiProfileMediaTrayView* tray_view_;

  DISALLOW_COPY_AND_ASSIGN(MultiProfileMediaTrayItem);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_MULTI_PROFILE_MEDIA_TRAY_ITEM_H_
