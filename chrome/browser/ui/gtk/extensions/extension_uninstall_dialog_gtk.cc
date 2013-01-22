// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Currently this file is only used for the uninstall prompt. The install prompt
// code is in extension_install_prompt2_gtk.cc.

#include "chrome/browser/extensions/extension_uninstall_dialog.h"

#include <gtk/gtk.h>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/gtk_util.h"

namespace {

// Left or right margin.
const int kPanelHorizMargin = 13;

// GTK implementation of the uninstall dialog.
class ExtensionUninstallDialogGtk : public ExtensionUninstallDialog {
 public:
  ExtensionUninstallDialogGtk(Profile* profile,
                              Browser* browser,
                              Delegate* delegate);
  virtual ~ExtensionUninstallDialogGtk() OVERRIDE;

 private:
  virtual void Show() OVERRIDE;

  CHROMEGTK_CALLBACK_1(ExtensionUninstallDialogGtk, void, OnResponse, int);

  GtkWidget* dialog_;
};

ExtensionUninstallDialogGtk::ExtensionUninstallDialogGtk(
    Profile* profile,
    Browser* browser,
    ExtensionUninstallDialog::Delegate* delegate)
    : ExtensionUninstallDialog(profile, browser, delegate),
      dialog_(NULL) {}

void ExtensionUninstallDialogGtk::Show() {
  BrowserWindow* browser_window = browser_->window();
  if (!browser_window) {
    delegate_->ExtensionUninstallCanceled();
    return;
  }

  // Build the dialog.
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_EXTENSION_UNINSTALL_PROMPT_TITLE).c_str(),
      browser_window->GetNativeWindow(),
      GTK_DIALOG_MODAL,
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CLOSE,
      l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON).c_str(),
      GTK_RESPONSE_ACCEPT,
      NULL);
#if !GTK_CHECK_VERSION(2, 22, 0)
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog_), FALSE);
#endif

  // Create a two column layout.
  GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog_));
  gtk_box_set_spacing(GTK_BOX(content_area), ui::kContentAreaSpacing);

  GtkWidget* icon_hbox = gtk_hbox_new(FALSE, ui::kContentAreaSpacing);
  gtk_box_pack_start(GTK_BOX(content_area), icon_hbox, TRUE, TRUE, 0);

  // Put Icon in the left column.
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(*icon_.bitmap());
  GtkWidget* icon = gtk_image_new_from_pixbuf(pixbuf);
  g_object_unref(pixbuf);
  gtk_box_pack_start(GTK_BOX(icon_hbox), icon, TRUE, TRUE, 0);

  // Create a new vbox for the right column.
  GtkWidget* right_column_area = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(icon_hbox), right_column_area, TRUE, TRUE, 0);

  std::string heading_text = l10n_util::GetStringFUTF8(
      IDS_EXTENSION_UNINSTALL_PROMPT_HEADING, UTF8ToUTF16(extension_->name()));
  GtkWidget* heading_label = gtk_label_new(heading_text.c_str());
  gtk_misc_set_alignment(GTK_MISC(heading_label), 0.0, 0.5);
  gtk_box_pack_start(GTK_BOX(right_column_area), heading_label, TRUE, TRUE, 0);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);
  gtk_widget_show_all(dialog_);
}

ExtensionUninstallDialogGtk::~ExtensionUninstallDialogGtk() {
  delegate_ = NULL;
  if (dialog_) {
    gtk_widget_destroy(dialog_);
    dialog_ = NULL;
  }
}

void ExtensionUninstallDialogGtk::OnResponse(
    GtkWidget* dialog, int response_id) {
  CHECK_EQ(dialog_, dialog);

  gtk_widget_destroy(dialog_);
  dialog_ = NULL;

  if (delegate_) {
    if (response_id == GTK_RESPONSE_ACCEPT)
      delegate_->ExtensionUninstallAccepted();
    else
      delegate_->ExtensionUninstallCanceled();
  }
}

}  // namespace

// static
// Platform specific implementation of the uninstall dialog show method.
ExtensionUninstallDialog* ExtensionUninstallDialog::Create(
    Profile* profile, Browser* browser, Delegate* delegate) {
  return new ExtensionUninstallDialogGtk(profile, browser, delegate);
}
