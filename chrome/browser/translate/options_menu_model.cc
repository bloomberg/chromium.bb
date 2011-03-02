// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/options_menu_model.h"

#include "base/metrics/histogram.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kAboutGoogleTranslateUrl[] =
#if defined(OS_CHROMEOS)
    "http://www.google.com/support/chromeos/bin/answer.py?answer=173424";
#else
    "http://www.google.com/support/chrome/bin/answer.py?answer=173424";
#endif

}  // namespace

OptionsMenuModel::OptionsMenuModel(
    TranslateInfoBarDelegate* translate_delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      translate_infobar_delegate_(translate_delegate) {
  string16 original_language = translate_delegate->GetLanguageDisplayableNameAt(
      translate_delegate->original_language_index());
  string16 target_language = translate_delegate->GetLanguageDisplayableNameAt(
      translate_delegate->target_language_index());

  // Populate the menu.
  // Incognito mode does not get any preferences related items.
  if (!translate_delegate->tab_contents()->profile()->IsOffTheRecord()) {
    AddCheckItem(IDC_TRANSLATE_OPTIONS_ALWAYS,
        l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_OPTIONS_ALWAYS,
            original_language, target_language));
    AddCheckItem(IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG,
        l10n_util::GetStringFUTF16(
            IDS_TRANSLATE_INFOBAR_OPTIONS_NEVER_TRANSLATE_LANG,
            original_language));
    AddCheckItem(IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE,
        l10n_util::GetStringUTF16(
            IDS_TRANSLATE_INFOBAR_OPTIONS_NEVER_TRANSLATE_SITE));
    AddSeparator();
  }
  AddItem(IDC_TRANSLATE_REPORT_BAD_LANGUAGE_DETECTION,
          l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_OPTIONS_REPORT_ERROR,
                                     original_language));
  AddItemWithStringId(IDC_TRANSLATE_OPTIONS_ABOUT,
      IDS_TRANSLATE_INFOBAR_OPTIONS_ABOUT);
}

OptionsMenuModel::~OptionsMenuModel() {
}

bool OptionsMenuModel::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG:
      return translate_infobar_delegate_->IsLanguageBlacklisted();

    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE:
      return translate_infobar_delegate_->IsSiteBlacklisted();

    case IDC_TRANSLATE_OPTIONS_ALWAYS:
      return translate_infobar_delegate_->ShouldAlwaysTranslate();

    default:
      NOTREACHED() << "Invalid command_id from menu";
      break;
  }
  return false;
}

bool OptionsMenuModel::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG :
    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE :
      return !translate_infobar_delegate_->ShouldAlwaysTranslate();

    case IDC_TRANSLATE_OPTIONS_ALWAYS :
      return (!translate_infobar_delegate_->IsLanguageBlacklisted() &&
          !translate_infobar_delegate_->IsSiteBlacklisted());

    default:
      break;
  }
  return true;
}

bool OptionsMenuModel::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) {
  return false;
}

void OptionsMenuModel::ExecuteCommand(int command_id) {
  switch (command_id) {
    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG:
      UMA_HISTOGRAM_COUNTS("Translate.NeverTranslateLang", 1);
      translate_infobar_delegate_->ToggleLanguageBlacklist();
      break;

    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE:
      UMA_HISTOGRAM_COUNTS("Translate.NeverTranslateSite", 1);
      translate_infobar_delegate_->ToggleSiteBlacklist();
      break;

    case IDC_TRANSLATE_OPTIONS_ALWAYS:
      UMA_HISTOGRAM_COUNTS("Translate.AlwaysTranslateLang", 1);
      translate_infobar_delegate_->ToggleAlwaysTranslate();
      break;

    case IDC_TRANSLATE_REPORT_BAD_LANGUAGE_DETECTION:
      translate_infobar_delegate_->ReportLanguageDetectionError();
      break;

    case IDC_TRANSLATE_OPTIONS_ABOUT: {
      TabContents* tab_contents = translate_infobar_delegate_->tab_contents();
      if (tab_contents) {
        GURL about_url = google_util::AppendGoogleLocaleParam(
            GURL(kAboutGoogleTranslateUrl));
        tab_contents->OpenURL(
            about_url, GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
      }
      break;
    }

    default:
      NOTREACHED() << "Invalid command id from menu.";
      break;
  }
}
