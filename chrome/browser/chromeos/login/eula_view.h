// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EULA_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EULA_VIEW_H_

#include <string>

#include "chrome/browser/chromeos/login/view_screen.h"
#include "views/controls/button/button.h"
#include "views/controls/link.h"
#include "views/view.h"

namespace views {

class Checkbox;
class Label;
class Link;
class NativeButton;
class Textfield;

}  // namespace views

namespace chromeos {

class EulaView
    : public views::View,
      public views::ButtonListener,
      public views::LinkController {
 public:
  explicit EulaView(chromeos::ScreenObserver* observer);
  virtual ~EulaView();

  // Initialize view controls and layout.
  void Init();

  // Update strings from the resources. Executed on language change.
  void UpdateLocalizedStrings();

 protected:
  // views::View implementation.
  virtual void LocaleChanged();

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::LinkController implementation.
  void LinkActivated(views::Link* source, int event_flags);

 private:
  // Dialog controls.
  views::Textfield* google_eula_text_;
  views::Checkbox* usage_statistics_checkbox_;
  views::Link* learn_more_link_;
  views::Textfield* oem_eula_text_;
  views::Link* system_security_settings_link_;
  views::NativeButton* cancel_button_;
  views::NativeButton* continue_button_;

  chromeos::ScreenObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(EulaView);
};

class EulaScreen : public DefaultViewScreen<EulaView> {
 public:
  explicit EulaScreen(WizardScreenDelegate* delegate)
      : DefaultViewScreen<EulaView>(delegate) {
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(EulaScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EULA_VIEW_H_
