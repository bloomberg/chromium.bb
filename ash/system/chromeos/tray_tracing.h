// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRACING_H_
#define ASH_SYSTEM_TRAY_TRACING_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_image_item.h"

namespace views {
class View;
}

namespace ash {

class ASH_EXPORT TracingObserver {
 public:
  virtual ~TracingObserver() {}

  // Notifies when tracing mode changes.
  virtual void OnTracingModeChanged(bool value) = 0;
};

// This is the item that displays when users enable performance tracing at
// chrome://slow.  It alerts them that this mode is running, and provides an
// easy way to open the page to disable it.
class TrayTracing : public TrayImageItem,
                    public TracingObserver {
 public:
  explicit TrayTracing(SystemTray* system_tray);
  ~TrayTracing() override;

 private:
  void SetTrayIconVisible(bool visible);

  // Overridden from TrayImageItem.
  bool GetInitialVisibility() override;
  views::View* CreateDefaultView(user::LoginStatus status) override;
  views::View* CreateDetailedView(user::LoginStatus status) override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;

  // Overridden from TracingObserver.
  void OnTracingModeChanged(bool value) override;

  views::View* default_;

  DISALLOW_COPY_AND_ASSIGN(TrayTracing);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRACING_H_
