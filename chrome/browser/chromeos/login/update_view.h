// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_

#include "base/timer.h"
#include "chrome/browser/google_update.h"
#include "views/view.h"

namespace views {
class Label;
class ProgressBar;
}  // namespace views

namespace chromeos {

class ScreenObserver;

// View for the network selection/initial welcome screen.
class UpdateView : public views::View,
                   public GoogleUpdateStatusListener {
 public:
  explicit UpdateView(chromeos::ScreenObserver* observer);
  virtual ~UpdateView();

  void Init();
  void UpdateLocalizedStrings();

  // views::View implementation:
  virtual void Layout();
  virtual bool AcceleratorPressed(const views::Accelerator& a);

  // Overridden from GoogleUpdateStatusListener:
  virtual void OnReportResults(GoogleUpdateUpgradeResult result,
                               GoogleUpdateErrorCode error_code,
                               const std::wstring& version);

  // Method that is called by WizardController to start update.
  void StartUpdate();

 private:
  // Method that reports update results to ScreenObserver.
  void ExitUpdate();

  // Timer notification handler.
  void OnMinimalUpdateTimeElapsed();

  // Accelerators.
  views::Accelerator escape_accelerator_;

  // Dialog controls.
  views::Label* installing_updates_label_;
  views::Label* escape_to_skip_label_;
  views::ProgressBar* progress_bar_;

  // Notifications receiver.
  chromeos::ScreenObserver* observer_;

  // Timer.
  base::OneShotTimer<UpdateView> minimal_update_time_timer_;

  // Update status.
  GoogleUpdateUpgradeResult update_result_;
  GoogleUpdateErrorCode update_error_;

  // Google Updater.
  scoped_refptr<GoogleUpdate> google_updater_;

  DISALLOW_COPY_AND_ASSIGN(UpdateView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_
