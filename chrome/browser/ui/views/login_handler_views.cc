// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_handler.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/login_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
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
    chrome::RecordDialogCreation(chrome::DialogIdentifier::LOGIN_HANDLER);
  }

  // LoginModelObserver:
  void OnAutofillDataAvailableInternal(
      const base::string16& username,
      const base::string16& password) override {
    // Nothing to do here since LoginView takes care of autofill for win.
  }
  void OnLoginModelDestroying() override {}

  // views::DialogDelegate:
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override {
    if (button == ui::DIALOG_BUTTON_OK)
      return l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL);
    return DialogDelegate::GetDialogButtonLabel(button);
  }

  base::string16 GetWindowTitle() const override {
    return l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_TITLE);
  }

  void WindowClosing() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::WebContents* web_contents = GetWebContentsForLogin();
    if (web_contents)
      web_contents->GetRenderViewHost()->GetWidget()->SetIgnoreInputEvents(
          false);

    // Reference is no longer valid.
    dialog_ = NULL;
    CancelAuth();
  }

  void DeleteDelegate() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // The widget is going to delete itself; clear our pointer.
    dialog_ = NULL;
    ResetModel();

    ReleaseSoon();
  }

  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_CHILD; }

  bool Cancel() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CancelAuth();
    return true;
  }

  bool Accept() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    SetAuth(login_view_->GetUsername(), login_view_->GetPassword());
    return true;
  }

  views::View* GetInitiallyFocusedView() override {
    return login_view_->GetInitiallyFocusedView();
  }

  views::View* GetContentsView() override { return login_view_; }
  views::Widget* GetWidget() override { return login_view_->GetWidget(); }
  const views::Widget* GetWidget() const override {
    return login_view_->GetWidget();
  }

  // LoginHandler:
  void BuildViewImpl(const base::string16& authority,
                     const base::string16& explanation,
                     LoginModelData* login_model_data) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Create a new LoginView and set the model for it.  The model (password
    // manager) is owned by the WebContents, but the view is parented to the
    // browser window, so the view may be destroyed after the password
    // manager. The view listens for model destruction and unobserves
    // accordingly.
    login_view_ = new LoginView(authority, explanation, login_model_data);

    // Scary thread safety note: This can potentially be called *after* SetAuth
    // or CancelAuth (say, if the request was cancelled before the UI thread got
    // control).  However, that's OK since any UI interaction in those functions
    // will occur via an InvokeLater on the UI thread, which is guaranteed
    // to happen after this is called (since this was InvokeLater'd first).
    dialog_ = constrained_window::ShowWebModalDialogViews(
        this, GetWebContentsForLogin());
    NotifyAuthNeeded();
  }

  void CloseDialog() override {
    // The hosting widget may have been freed.
    if (dialog_)
      dialog_->Close();
  }

 private:
  friend class base::RefCountedThreadSafe<LoginHandlerViews>;
  friend class LoginPrompt;

  ~LoginHandlerViews() override {}

  // The LoginView that contains the user's login information.
  LoginView* login_view_;

  views::Widget* dialog_;

  DISALLOW_COPY_AND_ASSIGN(LoginHandlerViews);
};

namespace chrome {

LoginHandler* CreateLoginHandlerViews(net::AuthChallengeInfo* auth_info,
                                      net::URLRequest* request) {
  return new LoginHandlerViews(auth_info, request);
}

}  // namespace chrome
