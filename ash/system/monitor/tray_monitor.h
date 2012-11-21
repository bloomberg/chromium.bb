// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DATE_TRAY_MONITOR_H_
#define ASH_SYSTEM_DATE_TRAY_MONITOR_H_

#include <list>

#include "ash/system/tray/system_tray_item.h"
#include "base/process.h"
#include "base/timer.h"

namespace views {
class Label;
}

namespace ash {
namespace internal {

class TrayMonitor : public SystemTrayItem {
 public:
  explicit TrayMonitor(SystemTray* system_tray);
  virtual ~TrayMonitor();

 private:
  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;

  void OnTimer();
  void OnGotHandles(const std::list<base::ProcessHandle>& handles);

  views::Label* label_;
  base::RepeatingTimer<TrayMonitor> refresh_timer_;

  DISALLOW_COPY_AND_ASSIGN(TrayMonitor);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_DATE_TRAY_MONITOR_H_
