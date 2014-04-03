// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_UPDATE_H_
#define ASH_SYSTEM_TRAY_UPDATE_H_

#include "ash/system/tray/tray_image_item.h"
#include "ash/system/user/update_observer.h"
#include "base/memory/scoped_ptr.h"

namespace views {
class View;
}

namespace ash {

namespace tray {
class UpdateNagger;
}

class TrayUpdate : public TrayImageItem,
                   public UpdateObserver {
 public:
  explicit TrayUpdate(SystemTray* system_tray);
  virtual ~TrayUpdate();

 private:
  // Overridden from TrayImageItem.
  virtual bool GetInitialVisibility() OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;

  // Overridden from UpdateObserver.
  virtual void OnUpdateRecommended(UpdateSeverity severity) OVERRIDE;

  // Used to nag the user in case the tray has been hidden too long with an
  // unseen update notification.
  scoped_ptr<tray::UpdateNagger> nagger_;

  UpdateObserver::UpdateSeverity severity_;

  DISALLOW_COPY_AND_ASSIGN(TrayUpdate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_UPDATE_H_
