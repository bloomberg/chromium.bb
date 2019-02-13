// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_BAR_VIEW_H_
#define ASH_WM_DESKS_DESKS_BAR_VIEW_H_

#include "base/macros.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class NewDeskButton;

// A bar that resides at the top portion of the overview mode's ShieldView,
// which contains the virtual desks thumbnails, as well as the new desk button.
class DesksBarView : public views::View, public views::ButtonListener {
 public:
  DesksBarView();
  ~DesksBarView() override = default;

  static int GetBarHeight();

  // views::View:
  const char* GetClassName() const override;
  void Layout() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  NewDeskButton* new_desk_button_;

  DISALLOW_COPY_AND_ASSIGN(DesksBarView);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_BAR_VIEW_H_
