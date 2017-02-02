// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"

#include <utility>

#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_config.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"

namespace safe_browsing {

namespace {

// Used to keep track of which settings types have been initialized in
// |SettingsResetPromptModel|.
enum SettingsType : uint32_t {
  SETTINGS_TYPE_HOMEPAGE = 1 << 0,
  SETTINGS_TYPE_DEFAULT_SEARCH = 1 << 1,
  SETTINGS_TYPE_STARTUP_URLS = 1 << 2,
  SETTINGS_TYPE_ALL = SETTINGS_TYPE_HOMEPAGE | SETTINGS_TYPE_DEFAULT_SEARCH |
                      SETTINGS_TYPE_STARTUP_URLS,
};

const extensions::Extension* GetExtension(
    Profile* profile,
    const extensions::ExtensionId& extension_id) {
  return extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
      extension_id);
}

}  // namespace

SettingsResetPromptModel::SettingsResetPromptModel(
    Profile* profile,
    std::unique_ptr<SettingsResetPromptConfig> prompt_config,
    std::unique_ptr<ResettableSettingsSnapshot> settings_snapshot)
    : profile_(profile),
      prompt_config_(std::move(prompt_config)),
      settings_snapshot_(std::move(settings_snapshot)),
      settings_types_initialized_(0),
      homepage_reset_domain_id_(-1),
      homepage_reset_state_(NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED),
      default_search_reset_domain_id_(-1),
      default_search_reset_state_(NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED),
      startup_urls_reset_state_(NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED) {
  DCHECK(profile_);
  DCHECK(prompt_config_);
  DCHECK(settings_snapshot_);

  InitHomepageData();
  InitDefaultSearchData();
  InitStartupUrlsData();
  DCHECK_EQ(settings_types_initialized_, SETTINGS_TYPE_ALL);

  InitExtensionData();

  // TODO(alito): Figure out cases where settings cannot be reset, for example
  // due to policy or extensions that cannot be disabled.
}

SettingsResetPromptModel::~SettingsResetPromptModel() {}

bool SettingsResetPromptModel::ShouldPromptForReset() {
  return homepage_reset_state() == RESET_REQUIRED ||
         default_search_reset_state() == RESET_REQUIRED ||
         startup_urls_reset_state() == RESET_REQUIRED;
}

std::string SettingsResetPromptModel::homepage() const {
  return settings_snapshot_->homepage();
}

SettingsResetPromptModel::ResetState
SettingsResetPromptModel::homepage_reset_state() const {
  DCHECK(homepage_reset_state_ != RESET_REQUIRED ||
         homepage_reset_domain_id_ >= 0);
  return homepage_reset_state_;
}

std::string SettingsResetPromptModel::default_search() const {
  return settings_snapshot_->dse_url();
}

SettingsResetPromptModel::ResetState
SettingsResetPromptModel::default_search_reset_state() const {
  DCHECK(default_search_reset_state_ != RESET_REQUIRED ||
         default_search_reset_domain_id_ >= 0);
  return default_search_reset_state_;
}

const std::vector<GURL>& SettingsResetPromptModel::startup_urls() const {
  return startup_urls_;
}

const std::vector<GURL>& SettingsResetPromptModel::startup_urls_to_reset()
    const {
  return startup_urls_to_reset_;
}

SettingsResetPromptModel::ResetState
SettingsResetPromptModel::startup_urls_reset_state() const {
  return startup_urls_reset_state_;
}

const SettingsResetPromptModel::ExtensionMap&
SettingsResetPromptModel::extensions_to_disable() const {
  return extensions_to_disable_;
}

void SettingsResetPromptModel::InitHomepageData() {
  DCHECK(!(settings_types_initialized_ & SETTINGS_TYPE_HOMEPAGE));

  settings_types_initialized_ |= SETTINGS_TYPE_HOMEPAGE;

  // If the home button is not visible to the user, then the homepage setting
  // has no real user-visible effect.
  if (!settings_snapshot_->show_home_button())
    return;

  // We do not currently support resetting New Tab pages that are set by
  // extensions.
  if (settings_snapshot_->homepage_is_ntp())
    return;

  homepage_reset_domain_id_ =
      prompt_config_->UrlToResetDomainId(GURL(settings_snapshot_->homepage()));
  if (homepage_reset_domain_id_ < 0)
    return;

  homepage_reset_state_ = RESET_REQUIRED;
}

void SettingsResetPromptModel::InitDefaultSearchData() {
  DCHECK(!(settings_types_initialized_ & SETTINGS_TYPE_DEFAULT_SEARCH));

  settings_types_initialized_ |= SETTINGS_TYPE_DEFAULT_SEARCH;

  default_search_reset_domain_id_ =
      prompt_config_->UrlToResetDomainId(GURL(settings_snapshot_->dse_url()));
  if (default_search_reset_domain_id_ < 0)
    return;

  default_search_reset_state_ = RESET_REQUIRED;
}

void SettingsResetPromptModel::InitStartupUrlsData() {
  DCHECK(!(settings_types_initialized_ & SETTINGS_TYPE_STARTUP_URLS));

  settings_types_initialized_ |= SETTINGS_TYPE_STARTUP_URLS;

  // Only the URLS startup type is a candidate for resetting.
  if (settings_snapshot_->startup_type() == SessionStartupPref::URLS) {
    startup_urls_ = settings_snapshot_->startup_urls();
    for (const GURL& startup_url : settings_snapshot_->startup_urls()) {
      int reset_domain_id = prompt_config_->UrlToResetDomainId(startup_url);
      if (reset_domain_id >= 0) {
        startup_urls_reset_state_ = RESET_REQUIRED;
        startup_urls_to_reset_.push_back(startup_url);
        domain_ids_for_startup_urls_to_reset_.insert(reset_domain_id);
      }
    }
  }
}

// Populate |extensions_to_disable_| with all enabled extensions that override
// the settings whose values were determined to need resetting. Note that all
// extensions that override such settings are included in the list, not just the
// one that is currently actively overriding the setting, in order to ensure
// that default values can be restored. This function should be called after
// other Init*() functions.
void SettingsResetPromptModel::InitExtensionData() {
  DCHECK(settings_snapshot_);
  DCHECK_EQ(settings_types_initialized_, SETTINGS_TYPE_ALL);

  // |enabled_extensions()| is a container of [id, name] pairs.
  for (const auto& id_name : settings_snapshot_->enabled_extensions()) {
    // Just in case there are duplicates in the list of enabled extensions.
    if (extensions_to_disable_.find(id_name.first) !=
        extensions_to_disable_.end()) {
      continue;
    }

    const extensions::Extension* extension =
        GetExtension(profile_, id_name.first);
    if (!extension)
      continue;

    const extensions::SettingsOverrides* overrides =
        extensions::SettingsOverrides::Get(extension);
    if (!overrides)
      continue;

    if ((homepage_reset_state_ == RESET_REQUIRED && overrides->homepage) ||
        (default_search_reset_state_ == RESET_REQUIRED &&
         overrides->search_engine) ||
        (startup_urls_reset_state_ == RESET_REQUIRED &&
         !overrides->startup_pages.empty())) {
      ExtensionInfo extension_info(extension);
      extensions_to_disable_.insert(
          std::make_pair(extension_info.id, extension_info));
    }
  }
}

}  // namespace safe_browsing.
