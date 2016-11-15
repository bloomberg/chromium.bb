// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_DATE_DATE_DEFAULT_VIEW_H_
#define ASH_COMMON_SYSTEM_DATE_DATE_DEFAULT_VIEW_H_

#include "ash/ash_export.h"
#include "ash/common/login_status.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {
namespace tray {
class DateView;
}  // namespace tray

class SystemTrayItem;
class TrayPopupHeaderButton;

// The system tray bubble view with the date and buttons for help, lock and
// shutdown.
// TODO(tdanderson): Remove this class once material design is enabled by
// default. See crbug.com/614453.
class ASH_EXPORT DateDefaultView : public views::View,
                                   public views::ButtonListener {
 public:
  DateDefaultView(SystemTrayItem* owner, LoginStatus login);

  ~DateDefaultView() override;

  views::View* GetHelpButtonView();
  const views::View* GetShutdownButtonViewForTest() const;

  tray::DateView* GetDateView();
  const tray::DateView* GetDateView() const;

 private:
  // Overridden from views::ButtonListener.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  TrayPopupHeaderButton* help_button_;
  TrayPopupHeaderButton* shutdown_button_;
  TrayPopupHeaderButton* lock_button_;
  tray::DateView* date_view_;

  DISALLOW_COPY_AND_ASSIGN(DateDefaultView);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_DATE_DATE_DEFAULT_VIEW_H_
