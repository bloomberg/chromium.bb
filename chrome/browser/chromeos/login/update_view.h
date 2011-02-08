// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_
#pragma once

#include "views/view.h"

namespace views {
class Label;
class ProgressBar;
class Throbber;
}  // namespace views

namespace chromeos {

class ScreenObserver;
class UpdateController;

// View for the network selection/initial welcome screen.
class UpdateView : public views::View {
 public:
  explicit UpdateView(ScreenObserver* observer);
  virtual ~UpdateView();

  void Init();
  void Reset();
  void UpdateLocalizedStrings();

  // Sets update controller.
  void set_controller(UpdateController* controller) {
    controller_ = controller;
  }

  // Advances view's progress bar. Maximum progress is 100.
  void AddProgress(int progress);

  // Sets the current value for the progress bar. Maximum progress is 100.
  void SetProgress(int progress);

  // Shows label with instructions for user to do a manual reboot.
  // Usually is not called since we rely on API that will reboot after update.
  void ShowManualRebootInfo();

  // Whether curtain window with throbber and label in the center should
  // be shown.
  void ShowCurtain(bool show_curtain);

  // views::View implementation:
  virtual void Layout();

 private:
  // Creates Label control and adds it as a child.
  void InitLabel(views::Label** label);

  // Updates visibility of the elements.
  void UpdateVisibility();

  // Keyboard accelerator to allow cancelling update by hitting escape.
  views::Accelerator escape_accelerator_;

  // Dialog controls.
  views::Label* installing_updates_label_;
  views::Label* reboot_label_;
  views::Label* manual_reboot_label_;
  views::Label* escape_to_skip_label_;
  views::ProgressBar* progress_bar_;

  // Curtain views.
  views::Label* checking_label_;
  views::Throbber* throbber_;

  // Show curtain view?
  bool show_curtain_;

  // Show manual reboot label?
  bool show_manual_reboot_label_;

  // Notifications receiver.
  chromeos::ScreenObserver* observer_;
  // Update controller.
  chromeos::UpdateController* controller_;

  DISALLOW_COPY_AND_ASSIGN(UpdateView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UPDATE_VIEW_H_
