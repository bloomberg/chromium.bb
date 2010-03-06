// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_

#include "views/view.h"

namespace views {
class Label;
}  // namespace views

namespace chromeos {

class ScreenObserver;

// View for the network selection/initial welcome screen.
class UpdateView : public views::View {
 public:
  explicit UpdateView(chromeos::ScreenObserver* observer);
  virtual ~UpdateView();

  void Init();
  void UpdateLocalizedStrings();

  // views::View implementation:
  virtual void Layout();

 private:
  // Dialog controls.
  views::Label* installing_updates_label_;

  // Notifications receiver.
  chromeos::ScreenObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(UpdateView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_
