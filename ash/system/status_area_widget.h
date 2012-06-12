// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_WIDGET_H_
#define ASH_SYSTEM_STATUS_AREA_WIDGET_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/wm/shelf_auto_hide_behavior.h"
#include "ui/views/widget/widget.h"

namespace ash {

class ShellDelegate;
class SystemTray;
class SystemTrayDelegate;

namespace internal {

class StatusAreaWidgetDelegate;

class ASH_EXPORT StatusAreaWidget : public views::Widget {
 public:
  StatusAreaWidget();
  virtual ~StatusAreaWidget();

  // Creates the SystemTray.
  void CreateTrayViews(ShellDelegate* shell_delegate);

  // Destroys the system tray. Called before tearing down the windows to avoid
  // shutdown ordering issues.
  void Shutdown();

  void SetShelfAlignment(ShelfAlignment alignment);

  SystemTray* system_tray() { return system_tray_; }
  SystemTrayDelegate* system_tray_delegate() {
    return system_tray_delegate_.get();
  }

 private:
  void AddSystemTray(SystemTray* system_tray, ShellDelegate* shell_delegate);

  scoped_ptr<SystemTrayDelegate> system_tray_delegate_;
  // Weak pointers to View classes that are parented to StatusAreaWidget:
  internal::StatusAreaWidgetDelegate* widget_delegate_;
  SystemTray* system_tray_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidget);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_WIDGET_H_
