// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_config.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

base::string16 FormatUrlForDisplay(const GURL& url) {
  return base::i18n::GetDisplayStringInLTRDirectionality(
      url_formatter::FormatUrl(url));
}

base::string16 FormatSearchEngineForDisplay(const GURL& search_url) {
  return url_formatter::FormatUrlForSecurityDisplay(search_url);
}

// Helper function that chooses and, if necessary, formats a message string
// based on the number of extensions that need to be disabled.
base::string16 ChooseActionMessage(size_t extensions_to_disable,
                                   int no_extensions_message,
                                   int one_extension_message,
                                   int multiple_extensions_message) {
  if (extensions_to_disable == 0U)
    return l10n_util::GetStringUTF16(no_extensions_message);
  if (extensions_to_disable == 1U)
    return l10n_util::GetStringUTF16(one_extension_message);
  // If extensions_to_disable > 1U.
  return l10n_util::GetStringFUTF16(
      multiple_extensions_message,
      base::SizeTToString16(extensions_to_disable));
}

}  // namespace.

SettingsResetPromptController::LabelInfo::LabelInfo(LabelType type,
                                                    const base::string16& text)
    : type(type), text(text) {}

SettingsResetPromptController::LabelInfo::LabelInfo(LabelType type,
                                                    int message_id)
    : type(type), text(l10n_util::GetStringUTF16(message_id)) {}

SettingsResetPromptController::LabelInfo::~LabelInfo() {}

SettingsResetPromptController::SettingsResetPromptController(
    std::unique_ptr<SettingsResetPromptModel> model)
    : model_(std::move(model)) {
  DCHECK(model_);
  DCHECK(model_->ShouldPromptForReset());

  InitMainText();
  InitDetailsText();
}

SettingsResetPromptController::~SettingsResetPromptController() {}

base::string16 SettingsResetPromptController::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_SETTINGS_RESET_PROMPT_TITLE);
}

base::string16 SettingsResetPromptController::GetButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_SETTINGS_RESET_PROMPT_ACCEPT_BUTTON_LABEL);
}

base::string16 SettingsResetPromptController::GetShowDetailsLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_SETTINGS_RESET_PROMPT_SHOW_DETAILS_BUTTON_LABEL);
}

base::string16 SettingsResetPromptController::GetHideDetailsLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_SETTINGS_RESET_PROMPT_HIDE_DETAILS_BUTTON_LABEL);
}

const std::vector<SettingsResetPromptController::LabelInfo>&
SettingsResetPromptController::GetMainText() const {
  DCHECK(!main_text_.empty());
  return main_text_;
}

const std::vector<SettingsResetPromptController::LabelInfo>&
SettingsResetPromptController::GetDetailsText() const {
  DCHECK(!details_text_.empty());
  return details_text_;
}

void SettingsResetPromptController::Accept() {
  model_->PerformReset(
      base::Bind(&SettingsResetPromptController::OnInteractionDone,
                 base::Unretained(this)));
}

void SettingsResetPromptController::Cancel() {
  OnInteractionDone();
}

void SettingsResetPromptController::InitMainText() {
  DCHECK(main_text_.empty());

  const bool reset_search = model_->default_search_reset_state() ==
                            SettingsResetPromptModel::RESET_REQUIRED;
  const bool reset_startup_urls = model_->startup_urls_reset_state() ==
                                  SettingsResetPromptModel::RESET_REQUIRED;
  const bool reset_homepage = model_->homepage_reset_state() ==
                              SettingsResetPromptModel::RESET_REQUIRED;
  DCHECK(reset_search || reset_startup_urls || reset_homepage);

  base::string16 action_message;
  if (int{reset_search} + int{reset_startup_urls} + int{reset_homepage} > 1) {
    // When multiple settings need to be reset, display a bulleted list of
    // settings with a common explanation text.
    main_text_.push_back(
        LabelInfo(LabelInfo::PARAGRAPH,
                  IDS_SETTINGS_RESET_PROMPT_EXPLANATION_FOR_MULTIPLE_SETTINGS));
    if (reset_search) {
      main_text_.push_back(
          LabelInfo(LabelInfo::BULLET_ITEM,
                    IDS_SETTINGS_RESET_PROMPT_SEARCH_ENGINE_SETTING_NAME));
    }
    if (reset_startup_urls) {
      main_text_.push_back(
          LabelInfo(LabelInfo::BULLET_ITEM,
                    IDS_SETTINGS_RESET_PROMPT_STARTUP_PAGE_SETTING_NAME));
    }
    if (reset_homepage) {
      main_text_.push_back(
          LabelInfo(LabelInfo::BULLET_ITEM,
                    IDS_SETTINGS_RESET_PROMPT_HOMEPAGE_SETTING_NAME));
    }

    // Use a common action message asking if user wants to reset the settings
    // that were displayed in the list above.
    main_text_.push_back(LabelInfo(
        LabelInfo::PARAGRAPH,
        ChooseActionMessage(
            model_->extensions_to_disable().size(),
            IDS_SETTINGS_RESET_PROMPT_ACTION_EXPLANATION_FOR_MULTIPLE_SETTINGS_NO_EXTENSIONS,
            IDS_SETTINGS_RESET_PROMPT_ACTION_EXPLANATION_FOR_MULTIPLE_SETTINGS_ONE_EXTENSION,
            IDS_SETTINGS_RESET_PROMPT_ACTION_EXPLANATION_FOR_MULTIPLE_SETTINGS_MULTIPLE_EXTENSIONS)));
  } else {
    // When only one setting needs to be reset, display tailored explanation and
    // action message for each setting.
    if (reset_search) {
      main_text_.push_back(
          LabelInfo(LabelInfo::PARAGRAPH,
                    IDS_SETTINGS_RESET_PROMPT_EXPLANATION_FOR_SEARCH_ENGINE));
      main_text_.push_back(LabelInfo(
          LabelInfo::PARAGRAPH,
          ChooseActionMessage(
              model_->extensions_to_disable().size(),
              IDS_SETTINGS_RESET_PROMPT_ACTION_EXPLANATION_FOR_SEARCH_ENGINE_NO_EXTENSIONS,
              IDS_SETTINGS_RESET_PROMPT_ACTION_EXPLANATION_FOR_SEARCH_ENGINE_ONE_EXTENSION,
              IDS_SETTINGS_RESET_PROMPT_ACTION_EXPLANATION_FOR_SEARCH_ENGINE_MULTIPLE_EXTENSIONS)));
    }
    if (reset_startup_urls) {
      main_text_.push_back(
          LabelInfo(LabelInfo::PARAGRAPH,
                    IDS_SETTINGS_RESET_PROMPT_EXPLANATION_FOR_STARTUP_PAGE));
      main_text_.push_back(LabelInfo(
          LabelInfo::PARAGRAPH,
          ChooseActionMessage(
              model_->extensions_to_disable().size(),
              IDS_SETTINGS_RESET_PROMPT_ACTION_EXPLANATION_FOR_STARTUP_PAGE_NO_EXTENSIONS,
              IDS_SETTINGS_RESET_PROMPT_ACTION_EXPLANATION_FOR_STARTUP_PAGE_ONE_EXTENSION,
              IDS_SETTINGS_RESET_PROMPT_ACTION_EXPLANATION_FOR_STARTUP_PAGE_MULTIPLE_EXTENSIONS)));
    }
    if (reset_homepage) {
      main_text_.push_back(
          LabelInfo(LabelInfo::PARAGRAPH,
                    IDS_SETTINGS_RESET_PROMPT_EXPLANATION_FOR_HOMEPAGE));
      main_text_.push_back(LabelInfo(
          LabelInfo::PARAGRAPH,
          ChooseActionMessage(
              model_->extensions_to_disable().size(),
              IDS_SETTINGS_RESET_PROMPT_ACTION_EXPLANATION_FOR_HOMEPAGE_NO_EXTENSIONS,
              IDS_SETTINGS_RESET_PROMPT_ACTION_EXPLANATION_FOR_HOMEPAGE_ONE_EXTENSION,
              IDS_SETTINGS_RESET_PROMPT_ACTION_EXPLANATION_FOR_HOMEPAGE_MULTIPLE_EXTENSIONS)));
    }
  }
}

void SettingsResetPromptController::InitDetailsText() {
  DCHECK(details_text_.empty());

  // Add the text explaining which settings are going to be reset.
  details_text_.push_back(LabelInfo(
      LabelInfo::PARAGRAPH,
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_RESET_PROMPT_DETAILS_SECTION_SETTINGS_EXPLANATION)));

  // Enumerate the settings that are going to be reset.
  if (model_->default_search_reset_state() ==
      SettingsResetPromptModel::RESET_REQUIRED) {
    details_text_.push_back(LabelInfo(
        LabelInfo::PARAGRAPH,
        l10n_util::GetStringUTF16(
            IDS_SETTINGS_RESET_PROMPT_DETAILED_SEARCH_ENGINE_SETTING)));
    details_text_.push_back(
        LabelInfo(LabelInfo::BULLET_ITEM,
                  FormatSearchEngineForDisplay(model_->default_search())));
  }
  if (model_->homepage_reset_state() ==
      SettingsResetPromptModel::RESET_REQUIRED) {
    details_text_.push_back(
        LabelInfo(LabelInfo::PARAGRAPH,
                  l10n_util::GetStringUTF16(
                      IDS_SETTINGS_RESET_PROMPT_DETAILED_HOMEPAGE_SETTING)));
    details_text_.push_back(LabelInfo(LabelInfo::BULLET_ITEM,
                                      FormatUrlForDisplay(model_->homepage())));
  }
  if (model_->startup_urls_reset_state() ==
      SettingsResetPromptModel::RESET_REQUIRED) {
    details_text_.push_back(
        LabelInfo(LabelInfo::PARAGRAPH,
                  l10n_util::GetStringUTF16(
                      IDS_SETTINGS_RESET_PROMPT_DETAILED_STARTUP_SETTINGS)));
    for (const GURL& url : model_->startup_urls())
      details_text_.push_back(
          LabelInfo(LabelInfo::BULLET_ITEM, FormatUrlForDisplay(url)));
  }

  if (!model_->extensions_to_disable().empty()) {
    // Add the text explaining which extensions will be disabled.
    details_text_.push_back(LabelInfo(
        LabelInfo::PARAGRAPH,
        l10n_util::GetStringUTF16(
            IDS_SETTINGS_RESET_PROMPT_DETAILS_SECTION_EXTENSION_EXPLANATION)));

    for (const auto& item : model_->extensions_to_disable()) {
      const ExtensionInfo& extension_info = item.second;
      details_text_.push_back(LabelInfo(
          LabelInfo::BULLET_ITEM, base::UTF8ToUTF16(extension_info.name)));
    }
  }
}

void SettingsResetPromptController::OnInteractionDone() {
  // TODO(alito): Add metrics reporting here.
  delete this;
}

}  // namespace safe_browsing
