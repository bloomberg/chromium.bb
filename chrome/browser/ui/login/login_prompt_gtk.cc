// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_prompt.h"

#include <gtk/gtk.h>

#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/tab_contents/tab_contents_view_gtk.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/login/login_model.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/l10n/l10n_util.h"

using webkit_glue::PasswordForm;

// ----------------------------------------------------------------------------
// LoginHandlerGtk

// This class simply forwards the authentication from the LoginView (on
// the UI thread) to the net::URLRequest (on the I/O thread).
// This class uses ref counting to ensure that it lives until all InvokeLaters
// have been called.
class LoginHandlerGtk : public LoginHandler,
                        public ConstrainedWindowGtkDelegate {
 public:
  LoginHandlerGtk(net::AuthChallengeInfo* auth_info, net::URLRequest* request)
      : LoginHandler(auth_info, request),
        username_entry_(NULL),
        password_entry_(NULL),
        ok_(NULL) {
  }

  virtual ~LoginHandlerGtk() {
    root_.Destroy();
  }

  // LoginModelObserver implementation.
  virtual void OnAutofillDataAvailable(const std::wstring& username,
                                       const std::wstring& password) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
                                           const string16& explanation) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    root_.Own(gtk_vbox_new(FALSE, gtk_util::kContentAreaBorder));
    GtkWidget* label = gtk_label_new(UTF16ToUTF8(explanation).c_str());
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

    ok_ = gtk_button_new_from_stock(GTK_STOCK_OK);
    gtk_button_set_label(
        GTK_BUTTON(ok_),
        l10n_util::GetStringUTF8(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL).c_str());
    g_signal_connect(ok_, "clicked", G_CALLBACK(OnOKClickedThunk), this);
    gtk_box_pack_end(GTK_BOX(hbox), ok_, FALSE, FALSE, 0);

    GtkWidget* cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    g_signal_connect(cancel, "clicked", G_CALLBACK(OnCancelClickedThunk), this);
    gtk_box_pack_end(GTK_BOX(hbox), cancel, FALSE, FALSE, 0);

    g_signal_connect(root_.get(), "hierarchy-changed",
                     G_CALLBACK(OnPromptHierarchyChangedThunk), this);

    SetModel(manager);

    // Scary thread safety note: This can potentially be called *after* SetAuth
    // or CancelAuth (say, if the request was cancelled before the UI thread got
    // control).  However, that's OK since any UI interaction in those functions
    // will occur via an InvokeLater on the UI thread, which is guaranteed
    // to happen after this is called (since this was InvokeLater'd first).
    SetDialog(GetTabContentsForLogin()->CreateConstrainedDialog(this));

    NotifyAuthNeeded();
  }

  // Overridden from ConstrainedWindowGtkDelegate:
  virtual GtkWidget* GetWidgetRoot() {
    return root_.get();
  }

  virtual GtkWidget* GetFocusWidget() {
    return username_entry_;
  }

  virtual void DeleteDelegate() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    // The constrained window is going to delete itself; clear our pointer.
    SetDialog(NULL);
    SetModel(NULL);

    ReleaseSoon();
  }

 private:
  friend class LoginPrompt;

  CHROMEGTK_CALLBACK_0(LoginHandlerGtk, void, OnOKClicked);
  CHROMEGTK_CALLBACK_0(LoginHandlerGtk, void, OnCancelClicked);
  CHROMEGTK_CALLBACK_1(LoginHandlerGtk, void, OnPromptHierarchyChanged,
                       GtkWidget*);

  // The GtkWidgets that form our visual hierarchy:
  // The root container we pass to our parent.
  OwnedWidgetGtk root_;

  // GtkEntry widgets that the user types into.
  GtkWidget* username_entry_;
  GtkWidget* password_entry_;
  GtkWidget* ok_;

  DISALLOW_COPY_AND_ASSIGN(LoginHandlerGtk);
};

void LoginHandlerGtk::OnOKClicked(GtkWidget* sender) {
  SetAuth(
      UTF8ToUTF16(gtk_entry_get_text(GTK_ENTRY(username_entry_))),
      UTF8ToUTF16(gtk_entry_get_text(GTK_ENTRY(password_entry_))));
}

void LoginHandlerGtk::OnCancelClicked(GtkWidget* sender) {
  CancelAuth();
}

void LoginHandlerGtk::OnPromptHierarchyChanged(GtkWidget* sender,
                                               GtkWidget* previous_toplevel) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!GTK_WIDGET_TOPLEVEL(gtk_widget_get_toplevel(ok_)))
    return;

  // Now that we have attached ourself to the window, we can make our OK
  // button the default action and mess with the focus.
  GTK_WIDGET_SET_FLAGS(ok_, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(ok_);
}

// static
LoginHandler* LoginHandler::Create(net::AuthChallengeInfo* auth_info,
                                   net::URLRequest* request) {
  return new LoginHandlerGtk(auth_info, request);
}
