// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TILES_TILES_DEFAULT_VIEW_H_
#define ASH_COMMON_SYSTEM_TILES_TILES_DEFAULT_VIEW_H_

#include "ash/common/system/chromeos/shutdown_policy_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {
class SystemTrayItem;

// The container view for the tiles in the bottom row of the system menu
// (settings, help, lock, and power).
class TilesDefaultView : public views::View,
                         public views::ButtonListener,
                         public ShutdownPolicyObserver {
 public:
  explicit TilesDefaultView(SystemTrayItem* owner);
  ~TilesDefaultView() override;

  // Sets the layout manager and child views of |this|.
  // TODO(tdanderson|bruthig): Consider moving the layout manager
  // setup code to a location which can be shared by other system menu rows.
  void Init();

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // ShutdownPolicyObserver:
  void OnShutdownPolicyChanged(bool reboot_on_shutdown) override;

 private:
  // Helper function to add a separator line between two tiles.
  // TODO(tdanderson|bruthig): Consider moving this to a location which can be
  // shared by other system menu rows.
  void AddSeparator();

  SystemTrayItem* owner_;

  // Pointers to the child buttons of |this|. Note that some buttons may not
  // exist (depending on the user's current login status, for instance), in
  // which case the corresponding pointer will be null.
  views::Button* settings_button_;
  views::Button* help_button_;
  views::Button* lock_button_;
  views::Button* power_button_;

  base::WeakPtrFactory<TilesDefaultView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TilesDefaultView);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TILES_TILES_DEFAULT_VIEW_H_
