// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_VIEW_H_

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/language_switch_model.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/views/info_bubble.h"
#include "views/accelerator.h"
#include "views/controls/button/button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/link.h"
#include "views/controls/textfield/textfield.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"

namespace views {
class Label;
class NativeButton;
}  // namespace views

namespace chromeos {

class MessageBubble;
class ScreenObserver;

class LoginManagerView : public views::View,
                         public LoginStatusConsumer,
                         public views::Textfield::Controller,
                         public views::LinkController,
                         public views::ButtonListener,
                         public InfoBubbleDelegate {
 public:
  explicit LoginManagerView(ScreenObserver* observer);
  virtual ~LoginManagerView();

  // Initialize view layout.
  void Init();
  // Overrides observer for testing.
  void set_observer(ScreenObserver* observer) { observer_ = observer; }

  // Returns pointer to Authenticator object. Caller shouldn't delete it.
  // To be used in tests only.
  Authenticator* authenticator() { return authenticator_.get(); }

  // Returns error id for tests only.
  int error_id() { return error_id_; }

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

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from views::LinkController.
  virtual void LinkActivated(views::Link* source, int event_flags);

  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // Overridden from LoginStatusConsumer.
  virtual void OnLoginFailure(const std::string& error);
  virtual void OnLoginSuccess(const std::string& username,
                              const std::string& credentials);

  // Overridden from views::InfoBubbleDelegate.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape) {
    bubble_ = NULL;
  }
  virtual bool CloseOnEscape() { return true; }
  virtual bool FadeOutOnClose() { return false; }

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add, views::View *parent,
                                    views::View *child);

  virtual void NativeViewHierarchyChanged(bool attached,
                                          gfx::NativeView native_view,
                                          views::RootView* root_view);
  virtual void LocaleChanged();

 private:
  // Given a |username| and |password|, this method attempts to authenticate to
  // the Google accounts servers.
  // Returns true upon success and false on failure.
  bool Authenticate(const std::string& username,
                    const std::string& password);

  // Shows error message with the specified message id.
  // -1 stands for no error.
  void ShowError(int error_id);

  void FocusFirstField();

  views::Textfield* username_field_;
  views::Textfield* password_field_;
  views::Label* title_label_;
  views::NativeButton* sign_in_button_;
  views::Link* create_account_link_;
  views::MenuButton* languages_menubutton_;

  views::Accelerator accel_focus_user_;
  views::Accelerator accel_focus_pass_;

  // Pointer to shown message bubble. We don't need to delete it because
  // it will be deleted on bubble closing.
  MessageBubble* bubble_;

  // Notifications receiver.
  ScreenObserver* observer_;

  // String ID for the current error message.
  // Set to -1 if no messages is shown.
  int error_id_;

  ScopedRunnableMethodFactory<LoginManagerView> focus_grabber_factory_;

  // Indicates that this view was created when focus manager was unavailable
  // (on the hidden tab, for example).
  bool focus_delayed_;

  // True when login is in process.
  bool login_in_process_;

  scoped_refptr<Authenticator> authenticator_;
  LanguageSwitchModel language_switch_model_;

  DISALLOW_COPY_AND_ASSIGN(LoginManagerView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_VIEW_H_
