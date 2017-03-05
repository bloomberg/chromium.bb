// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_ENTERPRISE_TRAY_ENTERPRISE_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_ENTERPRISE_TRAY_ENTERPRISE_H_

#include "ash/common/system/chromeos/enterprise/enterprise_domain_observer.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/view_click_listener.h"
#include "base/macros.h"

namespace ash {
class LabelTrayView;
class SystemTray;

class TrayEnterprise : public SystemTrayItem,
                       public ViewClickListener,
                       public EnterpriseDomainObserver {
 public:
  explicit TrayEnterprise(SystemTray* system_tray);
  ~TrayEnterprise() override;

  // If message is not empty updates content of default view, otherwise hides
  // tray items.
  void UpdateEnterpriseMessage();

  // Overridden from SystemTrayItem.
  views::View* CreateDefaultView(LoginStatus status) override;
  void DestroyDefaultView() override;

  // Overridden from EnterpriseDomainObserver.
  void OnEnterpriseDomainChanged() override;

  // Overridden from ViewClickListener.
  void OnViewClicked(views::View* sender) override;

 private:
  LabelTrayView* tray_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayEnterprise);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_ENTERPRISE_TRAY_ENTERPRISE_H_
