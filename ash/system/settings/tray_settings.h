// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SETTINGS_TRAY_SETTINGS_H_
#define ASH_SYSTEM_SETTINGS_TRAY_SETTINGS_H_
#pragma once

#include "ash/system/tray/system_tray_item.h"

class TraySettings : public ash::SystemTrayItem {
 public:
  TraySettings() {
  }

  virtual ~TraySettings() {}

 private:
  // Overridden from ash::SystemTrayItem
  virtual views::View* CreateTrayView() OVERRIDE;
  virtual views::View* CreateDefaultView() OVERRIDE;
  virtual views::View* CreateDetailedView() OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TraySettings);
};

#endif  // ASH_SYSTEM_SETTINGS_TRAY_SETTINGS_H_
