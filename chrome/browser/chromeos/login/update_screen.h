// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_H_

#include "base/ref_counted.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/login/update_view.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/google_update.h"

namespace chromeos {

class UpdateController {
 public:
  // Starts update.
  virtual void StartUpdate() = 0;
  // Cancels pending update without error.
  virtual void CancelUpdate() = 0;
};

class UpdateScreen: public DefaultViewScreen<chromeos::UpdateView>,
                    public GoogleUpdateStatusListener,
                    public UpdateController {
 public:
  explicit UpdateScreen(WizardScreenDelegate* delegate);
  virtual ~UpdateScreen();

  // Overridden from GoogleUpdateStatusListener:
  virtual void OnReportResults(GoogleUpdateUpgradeResult result,
                               GoogleUpdateErrorCode error_code,
                               const std::wstring& version);

  // Overridden from UpdateController:
  virtual void StartUpdate();
  virtual void CancelUpdate();

  // Reports update results to the ScreenObserver.
  virtual void ExitUpdate();
  // Returns true if minimal update time has elapsed.
  virtual bool MinimalUpdateTimeElapsed();
  // Creates GoogleUpdate object.
  virtual GoogleUpdate* CreateGoogleUpdate();

 private:
  // Timer notification handler.
  void OnMinimalUpdateTimeElapsed();

  // Timer.
  base::OneShotTimer<UpdateScreen> minimal_update_time_timer_;

  // Update status.
  GoogleUpdateUpgradeResult update_result_;
  GoogleUpdateErrorCode update_error_;
  bool checking_for_update_;

  // Google Updater.
  scoped_refptr<GoogleUpdate> google_updater_;

  DISALLOW_COPY_AND_ASSIGN(UpdateScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_SCREEN_H_
