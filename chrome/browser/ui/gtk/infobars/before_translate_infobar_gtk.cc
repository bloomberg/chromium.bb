// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/before_translate_infobar_gtk.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/base/l10n/l10n_util.h"

BeforeTranslateInfoBar::BeforeTranslateInfoBar(
    InfoBarService* owner,
    TranslateInfoBarDelegate* delegate)
    : TranslateInfoBarBase(owner, delegate) {
}

BeforeTranslateInfoBar::~BeforeTranslateInfoBar() {
}

void BeforeTranslateInfoBar::InitWidgets() {
  TranslateInfoBarBase::InitWidgets();

  GtkWidget* hbox = gtk_hbox_new(FALSE, ui::kControlSpacing);
  gtk_util::CenterWidgetInHBox(hbox_, hbox, false, 0);
  size_t offset = 0;
  string16 text =
      l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_BEFORE_MESSAGE,
                                 string16(), &offset);

  gtk_box_pack_start(GTK_BOX(hbox),
                     CreateLabel(UTF16ToUTF8(text.substr(0, offset))),
                     FALSE, FALSE, 0);
  size_t original_language_index = GetDelegate()->original_language_index();
  size_t target_language_index = GetDelegate()->target_language_index();
  bool exclude_the_other = original_language_index != target_language_index;
  GtkWidget* combobox = CreateLanguageCombobox(
      original_language_index,
      exclude_the_other ? target_language_index :
                          TranslateInfoBarDelegate::kNoIndex);
  Signals()->Connect(combobox, "changed",
                     G_CALLBACK(&OnLanguageModifiedThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), combobox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox),
                     CreateLabel(UTF16ToUTF8(text.substr(offset))),
                     FALSE, FALSE, 0);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_TRANSLATE_INFOBAR_ACCEPT).c_str());
  Signals()->Connect(button, "clicked",
                     G_CALLBACK(&OnAcceptPressedThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_TRANSLATE_INFOBAR_DENY).c_str());
  Signals()->Connect(button, "clicked",
                     G_CALLBACK(&OnDenyPressedThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  TranslateInfoBarDelegate* delegate = GetDelegate();
  if (delegate->ShouldShowNeverTranslateShortcut()) {
    std::string label = l10n_util::GetStringFUTF8(
        IDS_TRANSLATE_INFOBAR_NEVER_TRANSLATE,
        delegate->language_name_at(delegate->original_language_index()));
    button = gtk_button_new_with_label(label.c_str());
    Signals()->Connect(button, "clicked",
                       G_CALLBACK(&OnNeverTranslatePressedThunk), this);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  }

  if (delegate->ShouldShowAlwaysTranslateShortcut()) {
    std::string label = l10n_util::GetStringFUTF8(
        IDS_TRANSLATE_INFOBAR_ALWAYS_TRANSLATE,
        delegate->language_name_at(delegate->original_language_index()));
    button = gtk_button_new_with_label(label.c_str());
    Signals()->Connect(button, "clicked",
                       G_CALLBACK(&OnAlwaysTranslatePressedThunk), this);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  }
}

bool BeforeTranslateInfoBar::ShowOptionsMenuButton() const {
  return true;
}

void BeforeTranslateInfoBar::OnLanguageModified(GtkWidget* sender) {
  GetDelegate()->set_original_language_index(
      GetLanguageComboboxActiveId(GTK_COMBO_BOX(sender)));
}

void BeforeTranslateInfoBar::OnAcceptPressed(GtkWidget* sender) {
  GetDelegate()->Translate();
}

void BeforeTranslateInfoBar::OnDenyPressed(GtkWidget* sender) {
  GetDelegate()->TranslationDeclined();
  RemoveSelf();
}

void BeforeTranslateInfoBar::OnNeverTranslatePressed(GtkWidget* sender) {
  GetDelegate()->NeverTranslatePageLanguage();
}

void BeforeTranslateInfoBar::OnAlwaysTranslatePressed(GtkWidget* sender) {
  GetDelegate()->AlwaysTranslatePageLanguage();
}
