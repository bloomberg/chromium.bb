// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/after_translate_infobar_gtk.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/base/l10n/l10n_util.h"

AfterTranslateInfoBar::AfterTranslateInfoBar(
    InfoBarService* owner,
    TranslateInfoBarDelegate* delegate)
    : TranslateInfoBarBase(owner, delegate),
      weak_factory_(this) {
}

AfterTranslateInfoBar::~AfterTranslateInfoBar() {
}

void AfterTranslateInfoBar::InitWidgets() {
  TranslateInfoBarBase::InitWidgets();

  bool swapped_language_combos = false;
  bool autodetermined_source_language =
      GetDelegate()->original_language_index() ==
      TranslateInfoBarDelegate::kNoIndex;

  std::vector<string16> strings;
  TranslateInfoBarDelegate::GetAfterTranslateStrings(
        &strings, &swapped_language_combos, autodetermined_source_language);
  DCHECK_EQ(autodetermined_source_language ? 2U : 3U, strings.size());

  GtkWidget* new_hbox = gtk_hbox_new(FALSE, ui::kControlSpacing);
  gtk_util::CenterWidgetInHBox(hbox(), new_hbox, false, 0);

  size_t original_language_index = GetDelegate()->original_language_index();
  size_t target_language_index = GetDelegate()->target_language_index();
  bool exclude_the_other = original_language_index != target_language_index;

  GtkWidget* original_lang_combo = NULL;
  if (!autodetermined_source_language) {
    original_lang_combo = CreateLanguageCombobox(
        original_language_index,
        exclude_the_other ?
            target_language_index : TranslateInfoBarDelegate::kNoIndex);
    signals()->Connect(original_lang_combo, "changed",
                       G_CALLBACK(&OnOriginalLanguageModifiedThunk), this);
  }
  GtkWidget* target_lang_combo = CreateLanguageCombobox(
      target_language_index,
      exclude_the_other ? original_language_index :
                          TranslateInfoBarDelegate::kNoIndex);
  signals()->Connect(target_lang_combo, "changed",
                     G_CALLBACK(&OnTargetLanguageModifiedThunk), this);

  gtk_box_pack_start(GTK_BOX(new_hbox), CreateLabel(UTF16ToUTF8(strings[0])),
                     FALSE, FALSE, 0);
  gtk_box_pack_start(
      GTK_BOX(new_hbox),
      (swapped_language_combos || autodetermined_source_language) ?
          target_lang_combo : original_lang_combo,
      FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(new_hbox), CreateLabel(UTF16ToUTF8(strings[1])),
                     FALSE, FALSE, 0);
  if (!autodetermined_source_language) {
    gtk_box_pack_start(GTK_BOX(new_hbox),
                       swapped_language_combos ?
                           original_lang_combo : target_lang_combo,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(new_hbox), CreateLabel(UTF16ToUTF8(strings[2])),
                       FALSE, FALSE, 0);
  }

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_TRANSLATE_INFOBAR_REVERT).c_str());
  signals()->Connect(button, "clicked",
                     G_CALLBACK(&OnRevertPressedThunk), this);
  gtk_box_pack_start(GTK_BOX(new_hbox), button, FALSE, FALSE, 0);
}

bool AfterTranslateInfoBar::ShowOptionsMenuButton() const {
  return true;
}

void AfterTranslateInfoBar::SetOriginalLanguage(size_t language_index) {
  GetDelegate()->UpdateOriginalLanguageIndex(language_index);
  GetDelegate()->Translate();
}

void AfterTranslateInfoBar::SetTargetLanguage(size_t language_index) {
  GetDelegate()->UpdateTargetLanguageIndex(language_index);
  GetDelegate()->Translate();
}

void AfterTranslateInfoBar::OnOriginalLanguageModified(GtkWidget* sender) {
  size_t index = GetLanguageComboboxActiveId(GTK_COMBO_BOX(sender));
  if (index == GetDelegate()->original_language_index())
    return;

  // Setting the language will lead to a new translation that is going to close
  // the infobar.  This is not OK to do this from the signal handler, so we'll
  // defer it.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&AfterTranslateInfoBar::SetOriginalLanguage,
                 weak_factory_.GetWeakPtr(),
                 index));
}

void AfterTranslateInfoBar::OnTargetLanguageModified(GtkWidget* sender) {
  size_t index = GetLanguageComboboxActiveId(GTK_COMBO_BOX(sender));
  if (index == GetDelegate()->target_language_index())
    return;

  // See comment in OnOriginalLanguageModified on why we use a task.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&AfterTranslateInfoBar::SetTargetLanguage,
                 weak_factory_.GetWeakPtr(),
                 index));
}

void AfterTranslateInfoBar::OnRevertPressed(GtkWidget* sender) {
  GetDelegate()->RevertTranslation();
}
