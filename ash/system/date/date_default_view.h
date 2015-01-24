// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DATE_DATE_DEFAULT_VIEW_H_
#define ASH_SYSTEM_DATE_DATE_DEFAULT_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/chromeos/shutdown_policy_observer.h"
#include "ash/system/user/login_status.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {
namespace tray {
class DateView;
}  // namespace tray

class TrayPopupHeaderButton;

class ASH_EXPORT DateDefaultView : public views::View,
                                   public views::ButtonListener,
                                   public ash::ShutdownPolicyObserver {
 public:
  explicit DateDefaultView(ash::user::LoginStatus login);

  ~DateDefaultView() override;

  views::View* GetHelpButtonView();
  const views::View* GetShutdownButtonViewForTest() const;

  tray::DateView* GetDateView();
  const tray::DateView* GetDateView() const;

  // ash::ShutdownPolicyObserver:
  void OnShutdownPolicyChanged(bool reboot_on_shutdown) override;

 private:
  // Overridden from views::ButtonListener.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  TrayPopupHeaderButton* help_button_;
  TrayPopupHeaderButton* shutdown_button_;
  TrayPopupHeaderButton* lock_button_;
  tray::DateView* date_view_;

  base::WeakPtrFactory<DateDefaultView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DateDefaultView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_DATE_DATE_DEFAULT_VIEW_H_
