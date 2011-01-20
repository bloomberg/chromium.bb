// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/js_modal_dialog_gtk.h"

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/app_modal_dialogs/js_modal_dialog.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/message_box_flags.h"

namespace {

// We stash pointers to widgets on the gtk_dialog so we can refer to them
// after dialog creation.
const char kPromptTextId[] = "chrome_prompt_text";
const char kSuppressCheckboxId[] = "chrome_suppress_checkbox";

// If there's a text entry in the dialog, get the text from the first one and
// return it.
std::wstring GetPromptText(GtkDialog* dialog) {
  GtkWidget* widget = static_cast<GtkWidget*>(
      g_object_get_data(G_OBJECT(dialog), kPromptTextId));
  if (widget)
    return UTF8ToWide(gtk_entry_get_text(GTK_ENTRY(widget)));
  return std::wstring();
}

// If there's a toggle button in the dialog, return the toggled state.
// Otherwise, return false.
bool ShouldSuppressJSDialogs(GtkDialog* dialog) {
  GtkWidget* widget = static_cast<GtkWidget*>(
      g_object_get_data(G_OBJECT(dialog), kSuppressCheckboxId));
  if (widget)
    return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  return false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// JSModalDialogGtk, public:

JSModalDialogGtk::JSModalDialogGtk(JavaScriptAppModalDialog* dialog,
                                   gfx::NativeWindow parent_window)
    : dialog_(dialog) {
  GtkButtonsType buttons = GTK_BUTTONS_NONE;
  GtkMessageType message_type = GTK_MESSAGE_OTHER;

  // We add in the OK button manually later because we want to focus it
  // explicitly.
  switch (dialog_->dialog_flags()) {
    case ui::MessageBoxFlags::kIsJavascriptAlert:
      buttons = GTK_BUTTONS_NONE;
      message_type = GTK_MESSAGE_WARNING;
      break;

    case ui::MessageBoxFlags::kIsJavascriptConfirm:
      if (dialog_->is_before_unload_dialog()) {
        // onbeforeunload also uses a confirm prompt, it just has custom
        // buttons.  We add the buttons using gtk_dialog_add_button below.
        buttons = GTK_BUTTONS_NONE;
      } else {
        buttons = GTK_BUTTONS_CANCEL;
      }
      message_type = GTK_MESSAGE_QUESTION;
      break;

    case ui::MessageBoxFlags::kIsJavascriptPrompt:
      buttons = GTK_BUTTONS_CANCEL;
      message_type = GTK_MESSAGE_QUESTION;
      break;

    default:
      NOTREACHED();
  }

  // We want the alert to be app modal so put all the browser windows into the
  // same window group.
  gtk_util::MakeAppModalWindowGroup();

  gtk_dialog_ = gtk_message_dialog_new(parent_window,
      GTK_DIALOG_MODAL, message_type, buttons, "%s",
      WideToUTF8(dialog_->message_text()).c_str());
  gtk_util::ApplyMessageDialogQuirks(gtk_dialog_);
  gtk_window_set_title(GTK_WINDOW(gtk_dialog_),
                       WideToUTF8(dialog_->title()).c_str());

  // Adjust content area as needed.  Set up the prompt text entry or
  // suppression check box.
  if (ui::MessageBoxFlags::kIsJavascriptPrompt == dialog_->dialog_flags()) {
    // TODO(tc): Replace with gtk_dialog_get_content_area() when using GTK 2.14+
    GtkWidget* contents_vbox = GTK_DIALOG(gtk_dialog_)->vbox;
    GtkWidget* text_box = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(text_box),
        WideToUTF8(dialog_->default_prompt_text()).c_str());
    gtk_box_pack_start(GTK_BOX(contents_vbox), text_box, TRUE, TRUE, 0);
    g_object_set_data(G_OBJECT(gtk_dialog_), kPromptTextId, text_box);
    gtk_entry_set_activates_default(GTK_ENTRY(text_box), TRUE);
  }

  if (dialog_->display_suppress_checkbox()) {
    GtkWidget* contents_vbox = GTK_DIALOG(gtk_dialog_)->vbox;
    GtkWidget* check_box = gtk_check_button_new_with_label(
        l10n_util::GetStringUTF8(
        IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION).c_str());
    gtk_box_pack_start(GTK_BOX(contents_vbox), check_box, TRUE, TRUE, 0);
    g_object_set_data(G_OBJECT(gtk_dialog_), kSuppressCheckboxId, check_box);
  }

  // Adjust buttons/action area as needed.
  if (dialog_->is_before_unload_dialog()) {
    std::string button_text = l10n_util::GetStringUTF8(
      IDS_BEFOREUNLOAD_MESSAGEBOX_OK_BUTTON_LABEL);
    gtk_dialog_add_button(GTK_DIALOG(gtk_dialog_), button_text.c_str(),
        GTK_RESPONSE_OK);

    button_text = l10n_util::GetStringUTF8(
        IDS_BEFOREUNLOAD_MESSAGEBOX_CANCEL_BUTTON_LABEL);
    gtk_dialog_add_button(GTK_DIALOG(gtk_dialog_), button_text.c_str(),
        GTK_RESPONSE_CANCEL);
  } else {
    // Add the OK button and focus it.
    GtkWidget* ok_button = gtk_dialog_add_button(GTK_DIALOG(gtk_dialog_),
        GTK_STOCK_OK, GTK_RESPONSE_OK);
    if (ui::MessageBoxFlags::kIsJavascriptPrompt != dialog_->dialog_flags())
      gtk_widget_grab_focus(ok_button);
  }

  gtk_dialog_set_default_response(GTK_DIALOG(gtk_dialog_), GTK_RESPONSE_OK);
  g_signal_connect(gtk_dialog_, "response",
                   G_CALLBACK(OnDialogResponseThunk), this);
}

JSModalDialogGtk::~JSModalDialogGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// JSModalDialogGtk, NativeAppModalDialog implementation:

int JSModalDialogGtk::GetAppModalDialogButtons() const {
  switch (dialog_->dialog_flags()) {
    case ui::MessageBoxFlags::kIsJavascriptAlert:
      return ui::MessageBoxFlags::DIALOGBUTTON_OK;

    case ui::MessageBoxFlags::kIsJavascriptConfirm:
      return ui::MessageBoxFlags::DIALOGBUTTON_OK |
        ui::MessageBoxFlags::DIALOGBUTTON_CANCEL;

    case ui::MessageBoxFlags::kIsJavascriptPrompt:
      return ui::MessageBoxFlags::DIALOGBUTTON_OK;

    default:
      NOTREACHED();
      return 0;
  }
}

void JSModalDialogGtk::ShowAppModalDialog() {
  gtk_util::ShowModalDialogWithMinLocalizedWidth(GTK_WIDGET(gtk_dialog_),
      IDS_ALERT_DIALOG_WIDTH_CHARS);
}

void JSModalDialogGtk::ActivateAppModalDialog() {
  DCHECK(gtk_dialog_);
  gtk_window_present(GTK_WINDOW(gtk_dialog_));}

void JSModalDialogGtk::CloseAppModalDialog() {
  DCHECK(gtk_dialog_);
  OnDialogResponse(gtk_dialog_, GTK_RESPONSE_DELETE_EVENT);
}

void JSModalDialogGtk::AcceptAppModalDialog() {
  OnDialogResponse(gtk_dialog_, GTK_RESPONSE_OK);
}

void JSModalDialogGtk::CancelAppModalDialog() {
  OnDialogResponse(gtk_dialog_, GTK_RESPONSE_CANCEL);
}

////////////////////////////////////////////////////////////////////////////////
// JSModalDialogGtk, private:

void JSModalDialogGtk::OnDialogResponse(GtkWidget* dialog,
                                        int response_id) {
  switch (response_id) {
    case GTK_RESPONSE_OK:
      // The first arg is the prompt text and the second is true if we want to
      // suppress additional popups from the page.
      dialog_->OnAccept(GetPromptText(GTK_DIALOG(dialog)),
                        ShouldSuppressJSDialogs(GTK_DIALOG(dialog)));
      break;

    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_DELETE_EVENT:   // User hit the X on the dialog.
      dialog_->OnCancel(ShouldSuppressJSDialogs(GTK_DIALOG(dialog)));
      break;

    default:
      NOTREACHED();
  }
  gtk_widget_destroy(GTK_WIDGET(dialog));

  // Now that the dialog is gone, we can put all the  windows into separate
  // window groups so other dialogs are no longer app modal.
  gtk_util::AppModalDismissedUngroupWindows();
  delete this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeAppModalDialog, public:

// static
NativeAppModalDialog* NativeAppModalDialog::CreateNativeJavaScriptPrompt(
    JavaScriptAppModalDialog* dialog,
    gfx::NativeWindow parent_window) {
  return new JSModalDialogGtk(dialog, parent_window);
}
