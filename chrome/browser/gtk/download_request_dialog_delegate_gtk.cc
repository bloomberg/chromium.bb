// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/download_request_dialog_delegate_gtk.h"

#include "app/l10n_util.h"
#include "chrome/browser/gtk/constrained_window_gtk.h"
#include "chrome/common/gtk_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"

static const int kButtonSpacing = 3;

// static
DownloadRequestDialogDelegate* DownloadRequestDialogDelegate::Create(
    TabContents* tab,
    DownloadRequestManager::TabDownloadState* host) {
  return new DownloadRequestDialogDelegateGtk(tab, host);
}

DownloadRequestDialogDelegateGtk::~DownloadRequestDialogDelegateGtk() {
  root_.Destroy();
}

DownloadRequestDialogDelegateGtk::DownloadRequestDialogDelegateGtk(
    TabContents* tab,
    DownloadRequestManager::TabDownloadState* host)
    : DownloadRequestDialogDelegate(host),
      responded_(false) {
  // Create dialog.
  root_.Own(gtk_vbox_new(false, gtk_util::kContentAreaBorder));
  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_MULTI_DOWNLOAD_WARNING).c_str());
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_box_pack_start(GTK_BOX(root_.get()), label, FALSE, FALSE, 0);

  GtkWidget* hbox = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
  gtk_box_set_spacing(GTK_BOX(hbox), kButtonSpacing);
  gtk_box_pack_start(GTK_BOX(root_.get()), hbox, FALSE, FALSE, 0);

  GtkWidget* deny = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_MULTI_DOWNLOAD_WARNING_DENY).c_str());
  gtk_button_set_image(
      GTK_BUTTON(deny),
      gtk_image_new_from_stock(GTK_STOCK_CANCEL, GTK_ICON_SIZE_BUTTON));
  g_signal_connect(deny, "clicked", G_CALLBACK(OnDenyClickedThunk), this);
  gtk_box_pack_end(GTK_BOX(hbox), deny, FALSE, FALSE, 0);

  GtkWidget* allow = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_MULTI_DOWNLOAD_WARNING_ALLOW).c_str());
  gtk_button_set_image(
      GTK_BUTTON(allow),
      gtk_image_new_from_stock(GTK_STOCK_OK, GTK_ICON_SIZE_BUTTON));
  g_signal_connect(allow, "clicked", G_CALLBACK(OnAllowClickedThunk), this);
  gtk_box_pack_end(GTK_BOX(hbox), allow, FALSE, FALSE, 0);

  // Attach to window.
  window_ = tab->CreateConstrainedDialog(this);

  // Now that we have attached ourself to the window, we can make our deny
  // button the default action and mess with the focus.
  GTK_WIDGET_SET_FLAGS(deny, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(deny);
}

void DownloadRequestDialogDelegateGtk::CloseWindow() {
  window_->CloseConstrainedWindow();
}

GtkWidget* DownloadRequestDialogDelegateGtk::GetWidgetRoot() {
  return root_.get();
}

void DownloadRequestDialogDelegateGtk::DeleteDelegate() {
  if (!responded_)
    DoCancel();
  DCHECK(!host_);
  delete this;
}

void DownloadRequestDialogDelegateGtk::OnAllowClicked() {
  DoAccept();
  responded_ = true;
  CloseWindow();
}

void DownloadRequestDialogDelegateGtk::OnDenyClicked() {
  DoCancel();
  responded_ = true;
  CloseWindow();
}
