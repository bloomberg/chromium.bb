// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_VIEW_H_

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/version_loader.h"
#include "views/accelerator.h"
#include "views/controls/button/button.h"
#include "views/controls/textfield/textfield.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"

namespace views {
class Label;
class NativeButton;
}  // namespace views

namespace chromeos {

class ScreenObserver;

class LoginManagerView : public views::View,
                         public LoginStatusConsumer,
                         public views::Textfield::Controller,
                         public views::ButtonListener {
 public:
  explicit LoginManagerView(ScreenObserver* observer);
  virtual ~LoginManagerView();

  // Initialize view layout.
  void Init();

  // Update strings from the resources. Executed on language change.
  void UpdateLocalizedStrings();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void RequestFocus();

  // Overridden from views::WindowDelegate:
  virtual views::View* GetContentsView();

  // Setters for textfields.
  void SetUsername(const std::string& username);
  void SetPassword(const std::string& password);

  // Attempt to login with the current field values.
  void Login();

  // Overridden from views::Textfield::Controller
  // Not thread-safe, by virtue of using SetupSession().
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& keystroke);
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) {}

  // Overriden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overriden from LoginStatusConsumer.
  virtual void OnLoginFailure(const std::string error);
  virtual void OnLoginSuccess(const std::string username,
                              std::vector<std::string> cookies);

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add, views::View *parent,
                                    views::View *child);

  virtual void NativeViewHierarchyChanged(bool attached,
                                          gfx::NativeView native_view,
                                          views::RootView* root_view);

 private:
  // Given a |username| and |password|, this method attempts to authenticate to
  // the Google accounts servers.
  // Returns true upon success and false on failure.
  bool Authenticate(const std::string& username,
                    const std::string& password);

  // Callback from chromeos::VersionLoader giving the version.
  void OnOSVersion(VersionLoader::Handle handle,
                   std::string version);

  // Shows error message with the specified message id.
  // -1 stands for no error.
  void ShowError(int error_id);

  void FocusFirstField();

  views::Textfield* username_field_;
  views::Textfield* password_field_;
  views::Label* os_version_label_;
  views::Label* title_label_;
  views::Label* username_label_;
  views::Label* password_label_;
  views::Label* error_label_;
  views::NativeButton* sign_in_button_;
  views::NativeButton* create_account_button_;

  // Handles asynchronously loading the version.
  VersionLoader loader_;

  // Used to request the version.
  CancelableRequestConsumer consumer_;

  // Notifications receiver.
  ScreenObserver* observer_;

  // String ID for the current error message.
  // Set to -1 if no messages is shown.
  int error_id_;

  ScopedRunnableMethodFactory<LoginManagerView> focus_grabber_factory_;

  // Indicates that this view was created when focus manager was unavailable
  // (on the hidden tab, for example).
  bool focus_delayed_;

  scoped_ptr<Authenticator> authenticator_;

  DISALLOW_COPY_AND_ASSIGN(LoginManagerView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_VIEW_H_
