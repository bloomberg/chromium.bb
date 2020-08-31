// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SESSION_LOGOUT_BUTTON_TRAY_H_
#define ASH_SYSTEM_SESSION_LOGOUT_BUTTON_TRAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;

namespace views {
class MdTextButton;
}

namespace ash {
class Shelf;

// Adds a logout button to the shelf's status area if enabled by the
// kShowLogoutButtonInTray pref.
class ASH_EXPORT LogoutButtonTray : public TrayBackgroundView,
                                    public SessionObserver {
 public:
  explicit LogoutButtonTray(Shelf* shelf);
  ~LogoutButtonTray() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // TrayBackgroundView:
  void UpdateAfterLoginStatusChange() override;
  void UpdateLayout() override;
  void UpdateBackground() override;
  void ClickedOutsideBubble() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  base::string16 GetAccessibleNameForTray() override;
  const char* GetClassName() const override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  views::MdTextButton* button_for_test() const { return button_; }

 private:
  void UpdateShowLogoutButtonInTray();
  void UpdateLogoutDialogDuration();
  void UpdateVisibility();
  void UpdateButtonTextAndImage();

  views::MdTextButton* button_;
  bool show_logout_button_in_tray_ = false;
  base::TimeDelta dialog_duration_;

  // Observes user profile prefs.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(LogoutButtonTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_SESSION_LOGOUT_BUTTON_TRAY_H_
