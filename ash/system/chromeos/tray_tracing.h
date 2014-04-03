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
  virtual ~TrayTracing();

 private:
  void SetTrayIconVisible(bool visible);

  // Overridden from TrayImageItem.
  virtual bool GetInitialVisibility() OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;

  // Overridden from TracingObserver.
  virtual void OnTracingModeChanged(bool value) OVERRIDE;

  views::View* default_;

  DISALLOW_COPY_AND_ASSIGN(TrayTracing);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRACING_H_
