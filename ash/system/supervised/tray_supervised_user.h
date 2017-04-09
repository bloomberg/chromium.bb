// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SUPERVISED_TRAY_SUPERVISED_USER_H_
#define ASH_SYSTEM_SUPERVISED_TRAY_SUPERVISED_USER_H_

#include "ash/ash_export.h"
#include "ash/system/supervised/custodian_info_tray_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/macros.h"
#include "base/strings/string16.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {
class LabelTrayView;
class SystemTray;

class ASH_EXPORT TraySupervisedUser : public SystemTrayItem,
                                      public ViewClickListener,
                                      public CustodianInfoTrayObserver {
 public:
  explicit TraySupervisedUser(SystemTray* system_tray);
  ~TraySupervisedUser() override;

  // If message is not empty updates content of default view, otherwise hides
  // tray items.
  void UpdateMessage();

  // Overridden from SystemTrayItem.
  views::View* CreateDefaultView(LoginStatus status) override;
  void DestroyDefaultView() override;
  void UpdateAfterLoginStatusChange(LoginStatus status) override;

  // Overridden from ViewClickListener.
  void OnViewClicked(views::View* sender) override;

  // Overridden from CustodianInfoTrayObserver:
  void OnCustodianInfoChanged() override;

 private:
  friend class TraySupervisedUserTest;

  static const char kNotificationId[];

  void CreateOrUpdateNotification(const base::string16& new_message);

  void CreateOrUpdateSupervisedWarningNotification();

  const gfx::VectorIcon& GetSupervisedUserIcon() const;

  LabelTrayView* tray_view_;

  // Previous login status to avoid showing notification upon unlock.
  LoginStatus status_;

  // Previous user supervised state to avoid showing notification upon unlock.
  bool is_user_supervised_;

  DISALLOW_COPY_AND_ASSIGN(TraySupervisedUser);
};

}  // namespace ash

#endif  // ASH_SYSTEM_SUPERVISED_TRAY_SUPERVISED_USER_H_
