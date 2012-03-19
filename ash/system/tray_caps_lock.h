// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_CAPS_LOCK_H_
#define ASH_SYSTEM_TRAY_CAPS_LOCK_H_
#pragma once

#include "ash/system/tray/tray_image_item.h"

namespace views {
class ImageView;
}

namespace ash {

class ASH_EXPORT CapsLockObserver {
 public:
  virtual ~CapsLockObserver() {}

  virtual void OnCapsLockChanged(bool enabled) = 0;
};

namespace internal {

class TrayCapsLock : public TrayImageItem,
                     public CapsLockObserver {
 public:
  TrayCapsLock();
  virtual ~TrayCapsLock();

 private:
  // Overridden from TrayImageItem.
  virtual bool ShouldDisplay() OVERRIDE;

  // Overridden from CapsLockObserver.
  virtual void OnCapsLockChanged(bool enabled) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TrayCapsLock);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_CAPS_LOCK_H_


