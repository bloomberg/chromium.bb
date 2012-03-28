// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_CAPS_LOCK_H_
#define ASH_SYSTEM_TRAY_CAPS_LOCK_H_
#pragma once

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
                                 int string_id) = 0;
};

namespace internal {

class TrayCapsLock : public TrayImageItem,
                     public CapsLockObserver {
 public:
  TrayCapsLock();
  virtual ~TrayCapsLock();

 private:
  // Overridden from TrayImageItem.
  virtual bool GetInitialVisibility() OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;

  // Overridden from CapsLockObserver.
  virtual void OnCapsLockChanged(bool enabled,
                                 int string_id) OVERRIDE;

  scoped_ptr<views::View> detailed_;
  int string_id_;  // String ID for the string to show in the popup.

  DISALLOW_COPY_AND_ASSIGN(TrayCapsLock);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_CAPS_LOCK_H_
