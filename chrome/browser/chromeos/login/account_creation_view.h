// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_

#include <string>
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "views/accelerator.h"
#include "views/controls/button/button.h"
#include "views/controls/textfield/textfield.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window_delegate.h"

namespace views {
class GridLayout;
class Label;
class NativeButton;
}  // namespace views

namespace chromeos {
class ScreenObserver;
}  // namespace chromeos

class AccountCreationView : public WizardScreen,
                            public views::WindowDelegate,
                            public views::Textfield::Controller,
                            public views::ButtonListener {
 public:
  explicit AccountCreationView(chromeos::ScreenObserver* observer);
  virtual ~AccountCreationView();

  // WizardScreen implementation:
  virtual void Init();
  virtual void UpdateLocalizedStrings();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::WindowDelegate:
  virtual views::View* GetContentsView();

  // Overridden from views::Textfield::Controller:
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& keystroke);
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) {}

  // Overriden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 private:
  // Tries to create an account with user input.
  void CreateAccount();

  // Initializers for labels and textfields.
  void InitLabel(const gfx::Font& label_font, views::Label** label);
  void InitTextfield(const gfx::Font& field_font,
                     views::Textfield::StyleFlags style,
                     views::Textfield** field);

  // Initializes all controls on the screen.
  void InitControls();

  // Initializes the layout manager.
  void InitLayout();

  // Adds controls to layout manager.
  void BuildLayout(views::GridLayout* layout);

  // Controls on the view.
  views::Textfield* firstname_field_;
  views::Textfield* lastname_field_;
  views::Textfield* username_field_;
  views::Textfield* password_field_;
  views::Textfield* reenter_password_field_;
  views::Label* title_label_;
  views::Label* firstname_label_;
  views::Label* lastname_label_;
  views::Label* username_label_;
  views::Label* password_label_;
  views::Label* reenter_password_label_;
  views::NativeButton* create_account_button_;

  // Notifications receiver.
  chromeos::ScreenObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(AccountCreationView);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_

