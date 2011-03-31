// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/translate_message_infobar_gtk.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/gtk/gtk_util.h"

TranslateMessageInfoBar::TranslateMessageInfoBar(
    TranslateInfoBarDelegate* delegate)
    : TranslateInfoBarBase(delegate) {
}

TranslateMessageInfoBar::~TranslateMessageInfoBar() {
}

void TranslateMessageInfoBar::Init() {
  TranslateInfoBarBase::Init();

  GtkWidget* hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_util::CenterWidgetInHBox(hbox_, hbox, false, 0);

  std::string text = UTF16ToUTF8(GetDelegate()->GetMessageInfoBarText());
  gtk_box_pack_start(GTK_BOX(hbox), CreateLabel(text.c_str()), FALSE, FALSE, 0);
  string16 button_text = GetDelegate()->GetMessageInfoBarButtonText();
  if (!button_text.empty()) {
    GtkWidget* button =
        gtk_button_new_with_label(UTF16ToUTF8(button_text).c_str());
    g_signal_connect(button, "clicked",G_CALLBACK(&OnButtonPressedThunk), this);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  }
}

void TranslateMessageInfoBar::OnButtonPressed(GtkWidget* sender) {
  GetDelegate()->MessageInfoBarButtonPressed();
}
