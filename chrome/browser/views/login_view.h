// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_LOGIN_VIEW_H_
#define CHROME_BROWSER_VIEWS_LOGIN_VIEW_H_

#include "base/task.h"
#include "chrome/browser/login_model.h"
#include "views/view.h"

namespace views {
class Label;
class Textfield;
class LoginModel;
}  // namespace views

// This class is responsible for displaying the contents of a login window
// for HTTP/FTP authentication.
class LoginView : public views::View, public LoginModelObserver {
 public:
  explicit LoginView(const std::wstring& explanation);
  virtual ~LoginView();

  // Access the data in the username/password text fields.
  std::wstring GetUsername();
  std::wstring GetPassword();

  // LoginModelObserver implementation.
  virtual void OnAutofillDataAvailable(const std::wstring& username,
                                       const std::wstring& password);

  // Sets the model. This lets the observer notify the model
  // when it has been closed / freed, so the model should no longer try and
  // contact it. The view does not own the model, and it is the responsibility
  // of the caller to inform this view if the model is deleted.
  void SetModel(LoginModel* model);

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add, views::View *parent,
                                    views::View *child);

  virtual void NativeViewHierarchyChanged(bool attached,
                                          gfx::NativeView native_view,
                                          views::RootView* root_view);

 private:
  void FocusFirstField();

  // Non-owning refs to the input text fields.
  views::Textfield* username_field_;
  views::Textfield* password_field_;

  // Button labels
  views::Label* username_label_;
  views::Label* password_label_;

  // Authentication message.
  views::Label* message_label_;

  // If not null, points to a model we need to notify of our own destruction
  // so it doesn't try and access this when its too late.
  LoginModel* login_model_;

  ScopedRunnableMethodFactory<LoginView> focus_grabber_factory_;

  // Indicates that this view was created when focus manager was unavailable
  // (on the hidden tab, for example).
  bool focus_delayed_;

  DISALLOW_COPY_AND_ASSIGN(LoginView);
};

#endif  // CHROME_BROWSER_VIEWS_LOGIN_VIEW_H_
