// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_VIEW_H_

#include <string>
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

  void Init();
  void UpdateLocalizedStrings();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void RequestFocus();

  // Overridden from views::WindowDelegate:
  virtual views::View* GetContentsView();

  // Overridden from views::Textfield::Controller
  // Not thread-safe, by virtue of using SetupSession().
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& keystroke);
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) {}

  // Overriden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overriden from LoginStatusConsumer.
  virtual void OnLoginFailure();
  virtual void OnLoginSuccess(const std::string& username);

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

  // This is not threadsafe; as authentication is supposed to happen on the main
  // thread before any other threads are started, so this should be ok.
  // That said, the only reason we're not threadsafe right now is that we're
  // munging the CommandLine::ForCurrentProcess() to enable auto-client-side-ssl
  // for Googlers.  So, if we can do that differently to make this thread-safe,
  // that'd be A Good Thing (tm).
  void SetupSession(const std::string& username);

  // Callback from chromeos::VersionLoader giving the version.
  void OnOSVersion(VersionLoader::Handle handle,
                   std::string version);

  // Attempt to login with the current field values.
  void Login();

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
