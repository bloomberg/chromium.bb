// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/translate/before_translate_infobar_gtk.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

BeforeTranslateInfoBar::BeforeTranslateInfoBar(
    TranslateInfoBarDelegate* delegate)
    : TranslateInfoBarBase(delegate) {
}

BeforeTranslateInfoBar::~BeforeTranslateInfoBar() {
}

void BeforeTranslateInfoBar::Init() {
  TranslateInfoBarBase::Init();

  GtkWidget* hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_util::CenterWidgetInHBox(hbox_, hbox, false, 0);
  size_t offset = 0;
  string16 text =
      l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_BEFORE_MESSAGE,
                                 string16(), &offset);

  gtk_box_pack_start(GTK_BOX(hbox),
                     CreateLabel(UTF16ToUTF8(text.substr(0, offset))),
                     FALSE, FALSE, 0);
  GtkWidget* combobox =
      CreateLanguageCombobox(GetDelegate()->original_language_index(),
                             GetDelegate()->target_language_index());
  g_signal_connect(combobox, "changed",
                   G_CALLBACK(&OnLanguageModifiedThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), combobox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox),
                     CreateLabel(UTF16ToUTF8(text.substr(offset))),
                     FALSE, FALSE, 0);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_TRANSLATE_INFOBAR_ACCEPT).c_str());
  g_signal_connect(button, "clicked",G_CALLBACK(&OnAcceptPressedThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_TRANSLATE_INFOBAR_DENY).c_str());
  g_signal_connect(button, "clicked",G_CALLBACK(&OnDenyPressedThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  TranslateInfoBarDelegate* delegate = GetDelegate();
  if (delegate->ShouldShowNeverTranslateButton()) {
    std::string label =
        l10n_util::GetStringFUTF8(IDS_TRANSLATE_INFOBAR_NEVER_TRANSLATE,
                                  delegate->GetLanguageDisplayableNameAt(
                                      delegate->original_language_index()));
    button = gtk_button_new_with_label(label.c_str());
    g_signal_connect(button, "clicked",
                     G_CALLBACK(&OnNeverTranslatePressedThunk), this);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  }

  if (delegate->ShouldShowAlwaysTranslateButton()) {
    std::string label =
        l10n_util::GetStringFUTF8(IDS_TRANSLATE_INFOBAR_ALWAYS_TRANSLATE,
                                  delegate->GetLanguageDisplayableNameAt(
                                      delegate->original_language_index()));
    button = gtk_button_new_with_label(label.c_str());
    g_signal_connect(button, "clicked",
                     G_CALLBACK(&OnAlwaysTranslatePressedThunk), this);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  }
}

bool BeforeTranslateInfoBar::ShowOptionsMenuButton() const {
  return true;
}

void BeforeTranslateInfoBar::OnLanguageModified(GtkWidget* sender) {
  int index = GetLanguageComboboxActiveId(GTK_COMBO_BOX(sender));
  if (index == GetDelegate()->original_language_index())
    return;

  GetDelegate()->SetOriginalLanguage(index);
}

void BeforeTranslateInfoBar::OnAcceptPressed(GtkWidget* sender) {
  GetDelegate()->Translate();
}

void BeforeTranslateInfoBar::OnDenyPressed(GtkWidget* sender) {
  GetDelegate()->TranslationDeclined();
  RemoveInfoBar();
}

void BeforeTranslateInfoBar::OnNeverTranslatePressed(GtkWidget* sender) {
  GetDelegate()->NeverTranslatePageLanguage();
}

void BeforeTranslateInfoBar::OnAlwaysTranslatePressed(GtkWidget* sender) {
  GetDelegate()->AlwaysTranslatePageLanguage();
}
