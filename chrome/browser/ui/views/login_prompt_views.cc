// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_prompt.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/views/login_view.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

// ----------------------------------------------------------------------------
// LoginHandlerViews

// This class simply forwards the authentication from the LoginView (on
// the UI thread) to the net::URLRequest (on the I/O thread).
// This class uses ref counting to ensure that it lives until all InvokeLaters
// have been called.
class LoginHandlerViews : public LoginHandler, public views::DialogDelegate {
 public:
  LoginHandlerViews(net::AuthChallengeInfo* auth_info, net::URLRequest* request)
      : LoginHandler(auth_info, request),
        login_view_(NULL),
        dialog_(NULL) {
  }

  // LoginModelObserver:
  virtual void OnAutofillDataAvailable(
      const base::string16& username,
      const base::string16& password) OVERRIDE {
    // Nothing to do here since LoginView takes care of autofill for win.
  }
  virtual void OnLoginModelDestroying() OVERRIDE {}

  // views::DialogDelegate:
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const OVERRIDE {
    if (button == ui::DIALOG_BUTTON_OK)
      return l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL);
    return DialogDelegate::GetDialogButtonLabel(button);
  }

  virtual base::string16 GetWindowTitle() const OVERRIDE {
    return l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_TITLE);
  }

  virtual void WindowClosing() OVERRIDE {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::WebContents* web_contents = GetWebContentsForLogin();
    if (web_contents)
      web_contents->GetRenderViewHost()->SetIgnoreInputEvents(false);

    // Reference is no longer valid.
    dialog_ = NULL;
    CancelAuth();
  }

  virtual void DeleteDelegate() OVERRIDE {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // The widget is going to delete itself; clear our pointer.
    dialog_ = NULL;
    SetModel(NULL);

    ReleaseSoon();
  }

  virtual ui::ModalType GetModalType() const OVERRIDE {
    return ui::MODAL_TYPE_CHILD;
  }

  virtual bool Cancel() OVERRIDE {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CancelAuth();
    return true;
  }

  virtual bool Accept() OVERRIDE {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    SetAuth(login_view_->GetUsername(), login_view_->GetPassword());
    return true;
  }

  virtual views::View* GetInitiallyFocusedView() OVERRIDE {
    return login_view_->GetInitiallyFocusedView();
  }

  virtual views::View* GetContentsView() OVERRIDE {
    return login_view_;
  }
  virtual views::Widget* GetWidget() OVERRIDE {
    return login_view_->GetWidget();
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return login_view_->GetWidget();
  }

  // LoginHandler:
  virtual void BuildViewForPasswordManager(
      password_manager::PasswordManager* manager,
      const base::string16& explanation) OVERRIDE {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Create a new LoginView and set the model for it.  The model (password
    // manager) is owned by the WebContents, but the view is parented to the
    // browser window, so the view may be destroyed after the password
    // manager. The view listens for model destruction and unobserves
    // accordingly.
    login_view_ = new LoginView(explanation, manager);

    // Scary thread safety note: This can potentially be called *after* SetAuth
    // or CancelAuth (say, if the request was cancelled before the UI thread got
    // control).  However, that's OK since any UI interaction in those functions
    // will occur via an InvokeLater on the UI thread, which is guaranteed
    // to happen after this is called (since this was InvokeLater'd first).
    dialog_ = ShowWebModalDialogViews(this, GetWebContentsForLogin());
    NotifyAuthNeeded();
  }

  virtual void CloseDialog() OVERRIDE {
    // The hosting widget may have been freed.
    if (dialog_)
      dialog_->Close();
  }

 private:
  friend class base::RefCountedThreadSafe<LoginHandlerViews>;
  friend class LoginPrompt;

  virtual ~LoginHandlerViews() {}

  // The LoginView that contains the user's login information.
  LoginView* login_view_;

  views::Widget* dialog_;

  DISALLOW_COPY_AND_ASSIGN(LoginHandlerViews);
};

// static
LoginHandler* LoginHandler::Create(net::AuthChallengeInfo* auth_info,
                                   net::URLRequest* request) {
  return new LoginHandlerViews(auth_info, request);
}
