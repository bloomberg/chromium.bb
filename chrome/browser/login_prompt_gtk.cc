// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_prompt.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/gtk/constrained_window_gtk.h"
#include "chrome/browser/login_model.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request.h"

using webkit_glue::PasswordForm;

// ----------------------------------------------------------------------------
// LoginHandlerGtk

// This class simply forwards the authentication from the LoginView (on
// the UI thread) to the URLRequest (on the I/O thread).
// This class uses ref counting to ensure that it lives until all InvokeLaters
// have been called.
class LoginHandlerGtk : public LoginHandler,
                        public base::RefCountedThreadSafe<LoginHandlerGtk>,
                        public ConstrainedWindowGtkDelegate,
                        public LoginModelObserver {
 public:
  explicit LoginHandlerGtk(URLRequest* request)
      : handled_auth_(false),
        dialog_(NULL),
        request_(request),
        password_manager_(NULL),
        login_model_(NULL) {
    DCHECK(request_) << "LoginHandlerGtk constructed with NULL request";

    AddRef();  // matched by ReleaseLater.
    if (!ResourceDispatcherHost::RenderViewForRequest(request_,
                                                      &render_process_host_id_,
                                                      &tab_contents_id_)) {
      NOTREACHED();
    }
  }

  virtual ~LoginHandlerGtk() {
    if (login_model_)
      login_model_->SetObserver(NULL);
    root_.Destroy();
  }

  void SetModel(LoginModel* model) {
    if (login_model_)
      login_model_->SetObserver(NULL);
    login_model_ = model;
    if (login_model_)
      login_model_->SetObserver(this);
  }

  // LoginModelObserver implementation.
  virtual void OnAutofillDataAvailable(const std::wstring& username,
                                       const std::wstring& password) {
    // NOTE: Would be nice to use gtk_entry_get_text_length, but it is fairly
    // new and not always in our GTK version.
    if (strlen(gtk_entry_get_text(GTK_ENTRY(username_entry_))) == 0) {
      gtk_entry_set_text(GTK_ENTRY(username_entry_),
                         WideToUTF8(username).c_str());
      gtk_entry_set_text(GTK_ENTRY(password_entry_),
                         WideToUTF8(password).c_str());
      gtk_editable_select_region(GTK_EDITABLE(username_entry_), 0, -1);
    }
  }

  // LoginHandler:
  virtual void BuildViewForPasswordManager(PasswordManager* manager,
                                           std::wstring explanation) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

    root_.Own(gtk_vbox_new(NULL, gtk_util::kContentAreaBorder));
    GtkWidget* label = gtk_label_new(WideToUTF8(explanation).c_str());
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(root_.get()), label, FALSE, FALSE, 0);

    username_entry_ = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(username_entry_), TRUE);

    password_entry_ = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(password_entry_), TRUE);
    gtk_entry_set_visibility(GTK_ENTRY(password_entry_), FALSE);

    GtkWidget* table = gtk_util::CreateLabeledControlsGroup(NULL,
        l10n_util::GetStringUTF8(IDS_LOGIN_DIALOG_USERNAME_FIELD).c_str(),
        username_entry_,
        l10n_util::GetStringUTF8(IDS_LOGIN_DIALOG_PASSWORD_FIELD).c_str(),
        password_entry_,
        NULL);
    gtk_box_pack_start(GTK_BOX(root_.get()), table, FALSE, FALSE, 0);

    GtkWidget* hbox = gtk_hbox_new(FALSE, 12);
    gtk_box_pack_start(GTK_BOX(root_.get()), hbox, FALSE, FALSE, 0);

    GtkWidget* ok = gtk_button_new_from_stock(GTK_STOCK_OK);
    gtk_button_set_label(
        GTK_BUTTON(ok),
        l10n_util::GetStringUTF8(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL).c_str());
    g_signal_connect(ok, "clicked", G_CALLBACK(OnOKClicked), this);
    gtk_box_pack_end(GTK_BOX(hbox), ok, FALSE, FALSE, 0);

    GtkWidget* cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    g_signal_connect(cancel, "clicked", G_CALLBACK(OnCancelClicked), this);
    gtk_box_pack_end(GTK_BOX(hbox), cancel, FALSE, FALSE, 0);

    SetModel(manager);

    // Scary thread safety note: This can potentially be called *after* SetAuth
    // or CancelAuth (say, if the request was cancelled before the UI thread got
    // control).  However, that's OK since any UI interaction in those functions
    // will occur via an InvokeLater on the UI thread, which is guaranteed
    // to happen after this is called (since this was InvokeLater'd first).
    dialog_ = GetTabContentsForLogin()->CreateConstrainedDialog(this);

    // Now that we have attached ourself to the window, we can make our OK
    // button the default action and mess with the focus.
    GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
    gtk_widget_grab_default(ok);
    gtk_widget_grab_focus(username_entry_);

    SendNotifications();
  }

  virtual void SetPasswordForm(const webkit_glue::PasswordForm& form) {
    password_form_ = form;
  }

  virtual void SetPasswordManager(PasswordManager* password_manager) {
    password_manager_ = password_manager;
  }

  virtual TabContents* GetTabContentsForLogin() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

    return tab_util::GetTabContentsByID(render_process_host_id_,
                                        tab_contents_id_);
  }

  virtual void SetAuth(const std::wstring& username,
                       const std::wstring& password) {
    if (WasAuthHandled(true))
      return;

    // Tell the password manager the credentials were submitted / accepted.
    if (password_manager_) {
      password_form_.username_value = WideToUTF16Hack(username);
      password_form_.password_value = WideToUTF16Hack(password);
      password_manager_->ProvisionallySavePassword(password_form_);
    }

    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &LoginHandlerGtk::CloseContentsDeferred));
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &LoginHandlerGtk::SendNotifications));
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this, &LoginHandlerGtk::SetAuthDeferred, username,
        password));
  }

  virtual void CancelAuth() {
    if (WasAuthHandled(true))
      return;

    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &LoginHandlerGtk::CloseContentsDeferred));
    ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this, &LoginHandlerGtk::SendNotifications));
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this, &LoginHandlerGtk::CancelAuthDeferred));
  }

  virtual void OnRequestCancelled() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO)) <<
        "Why is OnRequestCancelled called from the UI thread?";

    // Reference is no longer valid.
    request_ = NULL;

    // Give up on auth if the request was cancelled.
    CancelAuth();
  }

  // Overridden from ConstrainedWindowGtkDelegate:
  virtual GtkWidget* GetWidgetRoot() {
    return root_.get();
  }

  virtual void DeleteDelegate() {
    if (!WasAuthHandled(true)) {
      ChromeThread::PostTask(
          ChromeThread::IO, FROM_HERE,
          NewRunnableMethod(this, &LoginHandlerGtk::CancelAuthDeferred));
      ChromeThread::PostTask(
          ChromeThread::UI, FROM_HERE,
          NewRunnableMethod(this, &LoginHandlerGtk::SendNotifications));
    }

    SetModel(NULL);

    // Delete this object once all InvokeLaters have been called.
    ChromeThread::ReleaseSoon(ChromeThread::IO, FROM_HERE, this);
  }

 private:
  friend class LoginPrompt;

  // Calls SetAuth from the IO loop.
  void SetAuthDeferred(const std::wstring& username,
                       const std::wstring& password) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

    if (request_) {
      request_->SetAuth(username, password);
      ResetLoginHandlerForRequest(request_);
    }
  }

  // Calls CancelAuth from the IO loop.
  void CancelAuthDeferred() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

    if (request_) {
      request_->CancelAuth();
      // Verify that CancelAuth does destroy the request via our delegate.
      DCHECK(request_ != NULL);
      ResetLoginHandlerForRequest(request_);
    }
  }

  // Closes the view_contents from the UI loop.
  void CloseContentsDeferred() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

    // The hosting ConstrainedWindow may have been freed.
    if (dialog_)
      dialog_->CloseConstrainedWindow();
  }

  // Returns whether authentication had been handled (SetAuth or CancelAuth).
  // If |set_handled| is true, it will mark authentication as handled.
  bool WasAuthHandled(bool set_handled) {
    AutoLock lock(handled_auth_lock_);
    bool was_handled = handled_auth_;
    if (set_handled)
      handled_auth_ = true;
    return was_handled;
  }

  // Notify observers that authentication is needed or received.  The automation
  // proxy uses this for testing.
  void SendNotifications() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

    NotificationService* service = NotificationService::current();
    TabContents* requesting_contents = GetTabContentsForLogin();
    if (!requesting_contents)
      return;

    NavigationController* controller = &requesting_contents->controller();

    if (!WasAuthHandled(false)) {
      LoginNotificationDetails details(this);
      service->Notify(NotificationType::AUTH_NEEDED,
                      Source<NavigationController>(controller),
                      Details<LoginNotificationDetails>(&details));
    } else {
      service->Notify(NotificationType::AUTH_SUPPLIED,
                      Source<NavigationController>(controller),
                      NotificationService::NoDetails());
    }
  }

  static void OnOKClicked(GtkButton *button, LoginHandlerGtk* handler) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

    handler->SetAuth(
        UTF8ToWide(gtk_entry_get_text(GTK_ENTRY(handler->username_entry_))),
        UTF8ToWide(gtk_entry_get_text(GTK_ENTRY(handler->password_entry_))));
  }

  static void OnCancelClicked(GtkButton *button, LoginHandlerGtk* handler) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    handler->CancelAuth();
  }

  // True if we've handled auth (SetAuth or CancelAuth has been called).
  bool handled_auth_;
  Lock handled_auth_lock_;

  // The ConstrainedWindow that is hosting our LoginView.
  // This should only be accessed on the UI loop.
  ConstrainedWindow* dialog_;

  // The request that wants login data.
  // This should only be accessed on the IO loop.
  URLRequest* request_;

  // The PasswordForm sent to the PasswordManager. This is so we can refer to it
  // when later notifying the password manager if the credentials were accepted
  // or rejected.
  // This should only be accessed on the UI loop.
  PasswordForm password_form_;

  // Points to the password manager owned by the TabContents requesting auth.
  // Can be null if the TabContents is not a TabContents.
  // This should only be accessed on the UI loop.
  PasswordManager* password_manager_;

  // Cached from the URLRequest, in case it goes NULL on us.
  int render_process_host_id_;
  int tab_contents_id_;

  // The GtkWidgets that form our visual hierarchy:
  // The root container we pass to our parent.
  OwnedWidgetGtk root_;

  // GtkEntry widgets that the user types into.
  GtkWidget* username_entry_;
  GtkWidget* password_entry_;

  // If not null, points to a model we need to notify of our own destruction
  // so it doesn't try and access this when its too late.
  LoginModel* login_model_;

  DISALLOW_COPY_AND_ASSIGN(LoginHandlerGtk);
};

// static
LoginHandler* LoginHandler::Create(URLRequest* request) {
  return new LoginHandlerGtk(request);
}
