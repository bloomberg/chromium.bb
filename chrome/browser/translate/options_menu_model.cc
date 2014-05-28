// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/options_menu_model.h"

#include "base/metrics/histogram.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_driver.h"
#include "grit/components_strings.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kAboutGoogleTranslateURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromeos/?p=ib_translation_bar";
#else
    "https://support.google.com/chrome/?p=ib_translation_bar";
#endif

}  // namespace

OptionsMenuModel::OptionsMenuModel(
    TranslateInfoBarDelegate* translate_delegate)
    : ui::SimpleMenuModel(this),
      translate_infobar_delegate_(translate_delegate) {
  // |translate_delegate| must already be owned.
  DCHECK(translate_infobar_delegate_->GetTranslateDriver());

  base::string16 original_language = translate_delegate->language_name_at(
      translate_delegate->original_language_index());
  base::string16 target_language = translate_delegate->language_name_at(
      translate_delegate->target_language_index());

  bool autodetermined_source_language =
      translate_delegate->original_language_index() ==
      TranslateInfoBarDelegate::kNoIndex;

  // Populate the menu.
  // Incognito mode does not get any preferences related items.
  if (!translate_delegate->is_off_the_record()) {
    if (!autodetermined_source_language) {
      AddCheckItem(IDC_TRANSLATE_OPTIONS_ALWAYS,
          l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_OPTIONS_ALWAYS,
                                     original_language, target_language));
      AddCheckItem(IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG,
          l10n_util::GetStringFUTF16(
              IDS_TRANSLATE_INFOBAR_OPTIONS_NEVER_TRANSLATE_LANG,
              original_language));
    }
    AddCheckItem(IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE,
        l10n_util::GetStringUTF16(
            IDS_TRANSLATE_INFOBAR_OPTIONS_NEVER_TRANSLATE_SITE));
    AddSeparator(ui::NORMAL_SEPARATOR);
  }
  if (!autodetermined_source_language) {
    AddItem(IDC_TRANSLATE_REPORT_BAD_LANGUAGE_DETECTION,
        l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_OPTIONS_REPORT_ERROR,
                                   original_language));
  }
  AddItemWithStringId(IDC_TRANSLATE_OPTIONS_ABOUT,
      IDS_TRANSLATE_INFOBAR_OPTIONS_ABOUT);
}

OptionsMenuModel::~OptionsMenuModel() {
}

bool OptionsMenuModel::IsCommandIdChecked(int command_id) const {
  switch (command_id) {
    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG:
      return !translate_infobar_delegate_->IsTranslatableLanguageByPrefs();

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
      return (translate_infobar_delegate_->IsTranslatableLanguageByPrefs() &&
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

void OptionsMenuModel::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG:
      translate_infobar_delegate_->ToggleTranslatableLanguageByPrefs();
      break;

    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE:
      translate_infobar_delegate_->ToggleSiteBlacklist();
      break;

    case IDC_TRANSLATE_OPTIONS_ALWAYS:
      translate_infobar_delegate_->ToggleAlwaysTranslate();
      break;

    case IDC_TRANSLATE_REPORT_BAD_LANGUAGE_DETECTION:
      translate_infobar_delegate_->ReportLanguageDetectionError();
      break;

    case IDC_TRANSLATE_OPTIONS_ABOUT: {
      TranslateDriver* translate_driver =
          translate_infobar_delegate_->GetTranslateDriver();
      if (translate_driver)
        translate_driver->OpenUrlInNewTab(GURL(kAboutGoogleTranslateURL));
      break;
    }

    default:
      NOTREACHED() << "Invalid command id from menu.";
      break;
  }
}
