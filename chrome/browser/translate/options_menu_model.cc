// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/options_menu_model.h"

#include "base/metrics/histogram.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

OptionsMenuModel::OptionsMenuModel(
    TranslateInfoBarDelegate* translate_delegate)
    : ui::SimpleMenuModel(this),
      translate_infobar_delegate_(translate_delegate) {
  // |translate_delegate| must already be owned.
  DCHECK(translate_infobar_delegate_->GetWebContents());

  base::string16 original_language = translate_delegate->language_name_at(
      translate_delegate->original_language_index());
  base::string16 target_language = translate_delegate->language_name_at(
      translate_delegate->target_language_index());

  bool autodetermined_source_language =
      translate_delegate->original_language_index() ==
      TranslateInfoBarDelegate::kNoIndex;

  // Populate the menu.
  // Incognito mode does not get any preferences related items.
  if (!translate_delegate->GetWebContents()->GetBrowserContext()->
      IsOffTheRecord()) {
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
      WebContents* web_contents = translate_infobar_delegate_->GetWebContents();
      if (web_contents) {
        OpenURLParams params(
            GURL(chrome::kAboutGoogleTranslateURL), Referrer(),
            NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK, false);
        web_contents->OpenURL(params);
      }
      break;
    }

    default:
      NOTREACHED() << "Invalid command id from menu.";
      break;
  }
}
