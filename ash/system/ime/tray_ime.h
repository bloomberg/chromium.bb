// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_IME_TRAY_IME_H_
#define ASH_SYSTEM_IME_TRAY_IME_H_
#pragma once

#include "ash/system/ime/ime_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/memory/scoped_ptr.h"

namespace views {
class Label;
}

namespace ash {

struct IMEInfo;

namespace internal {

namespace tray {
class IMEDefaultView;
class IMEDetailedView;
};

class TrayItemView;

class TrayIME : public SystemTrayItem,
                public IMEObserver {
 public:
  TrayIME();
  virtual ~TrayIME();

 private:
  void UpdateTrayLabel(const IMEInfo& info, size_t count);

  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;

  // Overridden from IMEObserver.
  virtual void OnIMERefresh() OVERRIDE;

  scoped_ptr<TrayItemView> tray_label_;
  scoped_ptr<tray::IMEDefaultView> default_;
  scoped_ptr<tray::IMEDetailedView> detailed_;

  DISALLOW_COPY_AND_ASSIGN(TrayIME);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_IME_TRAY_IME_H_
