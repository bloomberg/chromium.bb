// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/translate/after_translate_infobar_gtk.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

AfterTranslateInfoBar::AfterTranslateInfoBar(
    TranslateInfoBarDelegate* delegate)
    : TranslateInfoBarBase(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

AfterTranslateInfoBar::~AfterTranslateInfoBar() {
}

void AfterTranslateInfoBar::Init() {
  TranslateInfoBarBase::Init();

  bool swapped_language_combos = false;
  std::vector<string16> strings;
  TranslateInfoBarDelegate::GetAfterTranslateStrings(
      &strings, &swapped_language_combos);
  DCHECK(strings.size() == 3U);

  GtkWidget* hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_util::CenterWidgetInHBox(hbox_, hbox, false, 0);

  GtkWidget* original_lang_combo =
      CreateLanguageCombobox(GetDelegate()->original_language_index(),
                             GetDelegate()->target_language_index());
  g_signal_connect(original_lang_combo, "changed",
                   G_CALLBACK(&OnOriginalLanguageModifiedThunk), this);
  GtkWidget* target_lang_combo =
      CreateLanguageCombobox(GetDelegate()->target_language_index(),
                             GetDelegate()->original_language_index());
  g_signal_connect(target_lang_combo, "changed",
                   G_CALLBACK(&OnTargetLanguageModifiedThunk), this);

  gtk_box_pack_start(GTK_BOX(hbox), CreateLabel(UTF16ToUTF8(strings[0])),
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox),
                     swapped_language_combos ? target_lang_combo :
                                               original_lang_combo,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), CreateLabel(UTF16ToUTF8(strings[1])),
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox),
                     swapped_language_combos ? original_lang_combo :
                                               target_lang_combo,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), CreateLabel(UTF16ToUTF8(strings[2])),
                     FALSE, FALSE, 0);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_TRANSLATE_INFOBAR_REVERT).c_str());
  g_signal_connect(button, "clicked",G_CALLBACK(&OnRevertPressedThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
}

bool AfterTranslateInfoBar::ShowOptionsMenuButton() const {
  return true;
}

void AfterTranslateInfoBar::OnOriginalLanguageModified(GtkWidget* sender) {
  int index = GetLanguageComboboxActiveId(GTK_COMBO_BOX(sender));
  if (index == GetDelegate()->original_language_index())
    return;

  // Setting the language will lead to a new translation that is going to close
  // the infobar.  This is not OK to do this from the signal handler, so we'll
  // defer it.
  MessageLoop::current()->PostTask(FROM_HERE, method_factory_.NewRunnableMethod(
      &AfterTranslateInfoBar::SetOriginalLanguage, index));
}

void AfterTranslateInfoBar::OnTargetLanguageModified(GtkWidget* sender) {
  int index = GetLanguageComboboxActiveId(GTK_COMBO_BOX(sender));
  if (index == GetDelegate()->target_language_index())
    return;

  // See comment in OnOriginalLanguageModified on why we use a task.
  MessageLoop::current()->PostTask(FROM_HERE, method_factory_.NewRunnableMethod(
      &AfterTranslateInfoBar::SetTargetLanguage, index));
}

void AfterTranslateInfoBar::OnRevertPressed(GtkWidget* sender) {
  GetDelegate()->RevertTranslation();
}

void AfterTranslateInfoBar::SetOriginalLanguage(int language_index) {
  GetDelegate()->SetOriginalLanguage(language_index);
}

void AfterTranslateInfoBar::SetTargetLanguage(int language_index) {
  GetDelegate()->SetTargetLanguage(language_index);
}
