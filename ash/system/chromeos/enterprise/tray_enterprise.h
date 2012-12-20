// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_ENTERPRISE_TRAY_ENTERPRISE_H
#define ASH_SYSTEM_CHROMEOS_ENTERPRISE_TRAY_ENTERPRISE_H

#include "ash/system/chromeos/enterprise/enterprise_domain_observer.h"
#include "ash/system/tray/tray_image_item.h"
#include "ash/system/tray/tray_views.h"

namespace ash {
class SystemTray;
}

namespace ash {
namespace internal {

class EnterpriseDefaultView;

class TrayEnterprise : public TrayImageItem,
                       public ViewClickListener,
                       public EnterpriseDomainObserver {
 public:
  explicit TrayEnterprise(SystemTray* system_tray);
  virtual ~TrayEnterprise();

  // If message is not empty updates content of default view, otherwise hides
  // tray items.
  void UpdateEnterpriseMessage();

  // Overridden from TrayImageItem.
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual bool GetInitialVisibility() OVERRIDE;

  // Overridden from EnterpriseDomainObserver.
  virtual void OnEnterpriseDomainChanged() OVERRIDE;

  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE;

 private:
  EnterpriseDefaultView* default_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayEnterprise);
};

} // namespace internal
} // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_ENTERPRISE_TRAY_ENTERPRISE_H

