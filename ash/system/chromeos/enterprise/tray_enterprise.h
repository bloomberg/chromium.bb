// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_ENTERPRISE_TRAY_ENTERPRISE_H
#define ASH_SYSTEM_CHROMEOS_ENTERPRISE_TRAY_ENTERPRISE_H

#include "ash/system/chromeos/enterprise/enterprise_domain_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/view_click_listener.h"

namespace ash {
class SystemTray;
}

namespace ash {
namespace internal {

class EnterpriseDefaultView;

class TrayEnterprise : public SystemTrayItem,
                       public ViewClickListener,
                       public EnterpriseDomainObserver {
 public:
  explicit TrayEnterprise(SystemTray* system_tray);
  virtual ~TrayEnterprise();

  // If message is not empty updates content of default view, otherwise hides
  // tray items.
  void UpdateEnterpriseMessage();

  // Overridden from SystemTrayItem.
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;

  // Overridden from EnterpriseDomainObserver.
  virtual void OnEnterpriseDomainChanged() OVERRIDE;

  // Overridden from ViewClickListener.
  virtual void OnViewClicked(views::View* sender) OVERRIDE;

 private:
  EnterpriseDefaultView* default_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayEnterprise);
};

} // namespace internal
} // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_ENTERPRISE_TRAY_ENTERPRISE_H

