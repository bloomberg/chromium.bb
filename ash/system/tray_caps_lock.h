// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_CAPS_LOCK_H_
#define ASH_SYSTEM_TRAY_CAPS_LOCK_H_

#include "ash/system/tray/tray_image_item.h"

namespace views {
class ImageView;
class View;
}

namespace ash {

class ASH_EXPORT CapsLockObserver {
 public:
  virtual ~CapsLockObserver() {}

  virtual void OnCapsLockChanged(bool enabled,
                                 bool search_mapped_to_caps_lock) = 0;
};

namespace internal {

class CapsLockDefaultView;

class TrayCapsLock : public TrayImageItem,
                     public CapsLockObserver {
 public:
  explicit TrayCapsLock(SystemTray* system_tray);
  virtual ~TrayCapsLock();

 private:
  // Overridden from TrayImageItem.
  virtual bool GetInitialVisibility() OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;

  // Overridden from CapsLockObserver.
  virtual void OnCapsLockChanged(bool enabled,
                                 bool search_mapped_to_caps_lock) OVERRIDE;

  CapsLockDefaultView* default_;
  views::View* detailed_;

  bool search_mapped_to_caps_lock_;
  bool caps_lock_enabled_;
  bool message_shown_;

  DISALLOW_COPY_AND_ASSIGN(TrayCapsLock);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_CAPS_LOCK_H_
