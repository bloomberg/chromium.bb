// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_

#include "views/view.h"

namespace views {
class Label;
class ProgressBar;
}  // namespace views

namespace chromeos {

class ScreenObserver;
class UpdateController;

// View for the network selection/initial welcome screen.
class UpdateView : public views::View {
 public:
  explicit UpdateView(ScreenObserver* observer);
  virtual ~UpdateView();

  virtual void Init();
  virtual void Reset();
  virtual void UpdateLocalizedStrings();

  // Sets update controller.
  virtual void set_controller(UpdateController* controller) {
    controller_ = controller;
  }

  // Advances view's progress bar. Maximum progress is 100.
  virtual void AddProgress(int progress);

  // views::View implementation:
  virtual void Layout();
  virtual bool AcceleratorPressed(const views::Accelerator& a);

 private:
  // Creates Label control and adds it as a child.
  void InitLabel(views::Label** label);

  // Keyboard accelerator to allow cancelling update by hitting escape.
  views::Accelerator escape_accelerator_;

  // Dialog controls.
  views::Label* installing_updates_label_;
  views::Label* escape_to_skip_label_;
  views::ProgressBar* progress_bar_;

  // Notifications receiver.
  chromeos::ScreenObserver* observer_;
  // Update controller.
  chromeos::UpdateController* controller_;

  DISALLOW_COPY_AND_ASSIGN(UpdateView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_
