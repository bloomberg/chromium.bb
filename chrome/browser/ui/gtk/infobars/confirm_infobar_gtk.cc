// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/confirm_infobar_gtk.h"

#include <gtk/gtk.h>

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_chrome_shrinkable_hbox.h"
#include "chrome/browser/ui/gtk/gtk_util.h"

// ConfirmInfoBarDelegate ------------------------------------------------------

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  return new ConfirmInfoBarGtk(this);
}

// ConfirmInfoBarGtk -----------------------------------------------------------

ConfirmInfoBarGtk::ConfirmInfoBarGtk(ConfirmInfoBarDelegate* delegate)
    : InfoBar(delegate) {
  confirm_hbox_ = gtk_chrome_shrinkable_hbox_new(FALSE, FALSE,
                                                 kEndOfLabelSpacing);
  // This alignment allocates the confirm hbox only as much space as it
  // requests, and less if there is not enough available.
  GtkWidget* align = gtk_alignment_new(0, 0, 0, 1);
  gtk_container_add(GTK_CONTAINER(align), confirm_hbox_);
  gtk_box_pack_start(GTK_BOX(hbox_), align, TRUE, TRUE, 0);

  // We add the buttons in reverse order and pack end instead of start so
  // that the first widget to get shrunk is the label rather than the button(s).
  AddButton(ConfirmInfoBarDelegate::BUTTON_OK);
  AddButton(ConfirmInfoBarDelegate::BUTTON_CANCEL);

  std::string label_text = UTF16ToUTF8(delegate->GetMessageText());
  GtkWidget* label = gtk_label_new(label_text.c_str());
  gtk_util::ForceFontSizePixels(label, 13.4);
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
  gtk_util::CenterWidgetInHBox(confirm_hbox_, label, true, 0);
  gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &gtk_util::kGdkBlack);
  g_signal_connect(label, "map",
                   G_CALLBACK(gtk_util::InitLabelSizeRequestAndEllipsizeMode),
                   NULL);

  std::string link_text = UTF16ToUTF8(delegate->GetLinkText());
  if (link_text.empty())
    return;

  GtkWidget* link = gtk_chrome_link_button_new(link_text.c_str());
  gtk_misc_set_alignment(GTK_MISC(GTK_CHROME_LINK_BUTTON(link)->label), 0, 0.5);
  g_signal_connect(link, "clicked", G_CALLBACK(OnLinkClickedThunk), this);
  gtk_util::SetButtonTriggersNavigation(link);
  // Until we switch to vector graphics, force the font size.
  // 13.4px == 10pt @ 96dpi
  gtk_util::ForceFontSizePixels(GTK_CHROME_LINK_BUTTON(link)->label, 13.4);
  gtk_util::CenterWidgetInHBox(hbox_, link, true, kEndOfLabelSpacing);
}

ConfirmInfoBarGtk::~ConfirmInfoBarGtk() {
}

void ConfirmInfoBarGtk::AddButton(ConfirmInfoBarDelegate::InfoBarButton type) {
  if (delegate_->AsConfirmInfoBarDelegate()->GetButtons() & type) {
    GtkWidget* button = gtk_button_new_with_label(UTF16ToUTF8(
        delegate_->AsConfirmInfoBarDelegate()->GetButtonLabel(type)).c_str());
    gtk_util::CenterWidgetInHBox(confirm_hbox_, button, true, 0);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(type == ConfirmInfoBarDelegate::BUTTON_OK ?
                                OnOkButtonThunk : OnCancelButtonThunk),
                     this);
  }
}

void ConfirmInfoBarGtk::OnOkButton(GtkWidget* widget) {
  if (delegate_->AsConfirmInfoBarDelegate()->Accept())
    RemoveInfoBar();
}

void ConfirmInfoBarGtk::OnCancelButton(GtkWidget* widget) {
  if (delegate_->AsConfirmInfoBarDelegate()->Cancel())
    RemoveInfoBar();
}

void ConfirmInfoBarGtk::OnLinkClicked(GtkWidget* widget) {
  if (delegate_->AsConfirmInfoBarDelegate()->LinkClicked(
        gtk_util::DispositionForCurrentButtonPressEvent())) {
    RemoveInfoBar();
  }
}
