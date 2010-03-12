// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_

#include "base/timer.h"
#include "views/view.h"

namespace views {
class Label;
class ProgressBar;
}  // namespace views

namespace chromeos {

class ScreenObserver;

// View for the network selection/initial welcome screen.
class UpdateView : public views::View {
 public:
  explicit UpdateView(chromeos::ScreenObserver* observer);
  virtual ~UpdateView();

  void Init();
  void Refresh() {}
  void UpdateLocalizedStrings();

  // views::View implementation:
  virtual void Layout();

 private:
  // Timer notification handler.
  void OnTimerElapsed();

  // Dialog controls.
  views::Label* installing_updates_label_;
  views::ProgressBar* progress_bar_;

  // Notifications receiver.
  chromeos::ScreenObserver* observer_;

  // Timer.
  base::RepeatingTimer<UpdateView> timer_;

  DISALLOW_COPY_AND_ASSIGN(UpdateView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_
