// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/tab_modal_confirm_dialog_gtk.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "content/public/browser/notification_types.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

TabModalConfirmDialog* TabModalConfirmDialog::Create(
    TabModalConfirmDialogDelegate* delegate,
    content::WebContents* web_contents) {
  return new TabModalConfirmDialogGtk(delegate, web_contents);
}

TabModalConfirmDialogGtk::TabModalConfirmDialogGtk(
    TabModalConfirmDialogDelegate* delegate,
    content::WebContents* web_contents)
    : delegate_(delegate),
      window_(NULL) {
  dialog_ = gtk_vbox_new(FALSE, ui::kContentAreaSpacing);
  GtkWidget* label = gtk_label_new(
      UTF16ToUTF8(delegate->GetMessage()).c_str());
  gfx::Image* icon = delegate->GetIcon();
  GtkWidget* image = icon ? gtk_image_new_from_pixbuf(icon->ToGdkPixbuf())
                          : gtk_image_new_from_stock(GTK_STOCK_DIALOG_QUESTION,
                                                     GTK_ICON_SIZE_DIALOG);
  gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.0);

  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_label_set_selectable(GTK_LABEL(label), TRUE);

  GtkWidget* hbox = gtk_hbox_new(FALSE, ui::kControlSpacing);

  gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(dialog_), hbox, FALSE, FALSE, 0);

  GtkWidget* buttonBox = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_END);
  gtk_box_set_spacing(GTK_BOX(buttonBox), ui::kControlSpacing);
  gtk_box_pack_end(GTK_BOX(dialog_), buttonBox, FALSE, TRUE, 0);

  cancel_ = gtk_button_new_with_label(
      UTF16ToUTF8(delegate->GetCancelButtonTitle()).c_str());
  const char* cancel_button_icon_id = delegate->GetCancelButtonIcon();
  if (cancel_button_icon_id) {
    gtk_button_set_image(GTK_BUTTON(cancel_), gtk_image_new_from_stock(
        cancel_button_icon_id, GTK_ICON_SIZE_BUTTON));
  }
  g_signal_connect(cancel_, "clicked", G_CALLBACK(OnCancelThunk), this);
  gtk_box_pack_end(GTK_BOX(buttonBox), cancel_, FALSE, TRUE, 0);

  ok_ = gtk_button_new_with_label(
      UTF16ToUTF8(delegate->GetAcceptButtonTitle()).c_str());
  const char* accept_button_icon_id = delegate->GetAcceptButtonIcon();
  if (accept_button_icon_id) {
    gtk_button_set_image(GTK_BUTTON(ok_), gtk_image_new_from_stock(
        accept_button_icon_id, GTK_ICON_SIZE_BUTTON));
  }
  g_signal_connect(ok_, "clicked", G_CALLBACK(OnAcceptThunk), this);
  gtk_box_pack_end(GTK_BOX(buttonBox), ok_, FALSE, TRUE, 0);

  window_ = new ConstrainedWindowGtk(web_contents, this);
  delegate_->set_close_delegate(this);
}

GtkWidget* TabModalConfirmDialogGtk::GetWidgetRoot() {
  return dialog_;
}

GtkWidget* TabModalConfirmDialogGtk::GetFocusWidget() {
  return cancel_;
}

void TabModalConfirmDialogGtk::DeleteDelegate() {
  delete this;
}

TabModalConfirmDialogGtk::~TabModalConfirmDialogGtk() {
  gtk_widget_destroy(dialog_);
}

void TabModalConfirmDialogGtk::AcceptTabModalDialog() {
  OnAccept(NULL);
}

void TabModalConfirmDialogGtk::CancelTabModalDialog() {
  OnCancel(NULL);
}

void TabModalConfirmDialogGtk::CloseDialog() {
  window_->CloseWebContentsModalDialog();
}

void TabModalConfirmDialogGtk::OnAccept(GtkWidget* widget) {
  delegate_->Accept();
}

void TabModalConfirmDialogGtk::OnCancel(GtkWidget* widget) {
  delegate_->Cancel();
}
