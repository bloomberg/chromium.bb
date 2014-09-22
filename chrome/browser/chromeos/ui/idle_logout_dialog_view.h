// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_IDLE_LOGOUT_DIALOG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_UI_IDLE_LOGOUT_DIALOG_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/window/dialog_delegate.h"

namespace base {
class TimeDelta;
}
namespace views {
class Label;
}

namespace chromeos {

class IdleLogoutDialogView;
class KioskModeSettings;

// A class that holds the settings for IdleLogoutDialogView; this class
// can be overridden with a mock for testing.
class IdleLogoutSettingsProvider {
 public:
  IdleLogoutSettingsProvider();
  virtual ~IdleLogoutSettingsProvider();

  virtual base::TimeDelta GetCountdownUpdateInterval();
  virtual KioskModeSettings* GetKioskModeSettings();
  virtual void LogoutCurrentUser(IdleLogoutDialogView* dialog);

 private:
  DISALLOW_COPY_AND_ASSIGN(IdleLogoutSettingsProvider);
};

// A class to show the logout on idle dialog if the machine is in retail mode.
class IdleLogoutDialogView : public views::DialogDelegateView {
 public:
  static void ShowDialog();
  static void CloseDialog();

  // views::DialogDelegateView:
  virtual int GetDialogButtons() const OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual bool Close() OVERRIDE;

 private:
  friend class MockIdleLogoutSettingsProvider;
  friend class IdleLogoutDialogViewTest;
  FRIEND_TEST_ALL_PREFIXES(IdleLogoutDialogViewTest, ShowDialogAndCloseView);
  FRIEND_TEST_ALL_PREFIXES(IdleLogoutDialogViewTest,
                           ShowDialogAndCloseViewClose);

  IdleLogoutDialogView();
  virtual ~IdleLogoutDialogView();

  // Adds the labels and adds them to the layout.
  void InitAndShow();

  void Show();

  void UpdateCountdown();

  // For testing.
  static IdleLogoutDialogView* current_instance();
  static void set_settings_provider(IdleLogoutSettingsProvider* provider);

  views::Label* restart_label_;

  // Time at which the countdown is over and we should close the dialog.
  base::Time countdown_end_time_;

  base::RepeatingTimer<IdleLogoutDialogView> timer_;

  static IdleLogoutSettingsProvider* provider_;

  base::WeakPtrFactory<IdleLogoutDialogView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IdleLogoutDialogView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_IDLE_LOGOUT_DIALOG_VIEW_H_
