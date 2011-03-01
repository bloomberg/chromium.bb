// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_prompt.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/views/login_view.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/window/dialog_delegate.h"

using webkit_glue::PasswordForm;

// ----------------------------------------------------------------------------
// LoginHandlerWin

// This class simply forwards the authentication from the LoginView (on
// the UI thread) to the net::URLRequest (on the I/O thread).
// This class uses ref counting to ensure that it lives until all InvokeLaters
// have been called.
class LoginHandlerWin : public LoginHandler,
                        public ConstrainedDialogDelegate {
 public:
  LoginHandlerWin(net::AuthChallengeInfo* auth_info, net::URLRequest* request)
      : LoginHandler(auth_info, request) {
  }

  // LoginModelObserver implementation.
  virtual void OnAutofillDataAvailable(const std::wstring& username,
                                       const std::wstring& password) {
    // Nothing to do here since LoginView takes care of autofil for win.
  }

  void set_login_view(LoginView* login_view) {
    login_view_ = login_view;
  }

  // views::DialogDelegate methods:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const {
    if (button == MessageBoxFlags::DIALOGBUTTON_OK)
      return l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL);
    return DialogDelegate::GetDialogButtonLabel(button);
  }

  virtual std::wstring GetWindowTitle() const {
    return l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_TITLE);
  }

  virtual void WindowClosing() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    TabContents* tab = GetTabContentsForLogin();
    if (tab)
      tab->render_view_host()->set_ignore_input_events(false);

    // Reference is no longer valid.
    SetDialog(NULL);

    CancelAuth();
  }

  virtual void DeleteDelegate() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    // The constrained window is going to delete itself; clear our pointer.
    SetDialog(NULL);
    SetModel(NULL);

    ReleaseSoon();
  }

  virtual bool Cancel() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    CancelAuth();
    return true;
  }

  virtual bool Accept() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    SetAuth(login_view_->GetUsername(), login_view_->GetPassword());
    return true;
  }

  virtual views::View* GetContentsView() {
    return login_view_;
  }

  // LoginHandler:

  virtual void BuildViewForPasswordManager(PasswordManager* manager,
                                           const string16& explanation) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    TabContents* tab_contents = GetTabContentsForLogin();
    bool should_focus_view = !tab_contents->delegate() ||
        tab_contents->delegate()->ShouldFocusConstrainedWindow();

    LoginView* view = new LoginView(UTF16ToWideHack(explanation),
                                    should_focus_view);

    // Set the model for the login view. The model (password manager) is owned
    // by the view's parent TabContents, so natural destruction order means we
    // don't have to worry about calling SetModel(NULL), because the view will
    // be deleted before the password manager.
    view->SetModel(manager);

    set_login_view(view);

    // Scary thread safety note: This can potentially be called *after* SetAuth
    // or CancelAuth (say, if the request was cancelled before the UI thread got
    // control).  However, that's OK since any UI interaction in those functions
    // will occur via an InvokeLater on the UI thread, which is guaranteed
    // to happen after this is called (since this was InvokeLater'd first).
    SetDialog(GetTabContentsForLogin()->CreateConstrainedDialog(this));
    NotifyAuthNeeded();
  }

 private:
  friend class base::RefCountedThreadSafe<LoginHandlerWin>;
  friend class LoginPrompt;

  ~LoginHandlerWin() {}

  // The LoginView that contains the user's login information
  LoginView* login_view_;

  DISALLOW_COPY_AND_ASSIGN(LoginHandlerWin);
};

// static
LoginHandler* LoginHandler::Create(net::AuthChallengeInfo* auth_info,
                                   net::URLRequest* request) {
  return new LoginHandlerWin(auth_info, request);
}
