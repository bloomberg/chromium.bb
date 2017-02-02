// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_MODEL_H_
#define CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_MODEL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/extension_info.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

class Profile;
class ResettableSettingsSnapshot;

namespace safe_browsing {

class SettingsResetPromptConfig;

// Encapsulates information about settings needed for the settings reset prompt
// and implements the reset logic.
class SettingsResetPromptModel {
 public:
  enum ResetState {
    RESET_REQUIRED = 1,
    NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED = 2,
  };

  using ExtensionMap =
      std::unordered_map<extensions::ExtensionId, ExtensionInfo>;

  SettingsResetPromptModel(
      Profile* profile,
      std::unique_ptr<SettingsResetPromptConfig> prompt_config,
      std::unique_ptr<ResettableSettingsSnapshot> settings_snapshot);
  ~SettingsResetPromptModel();

  // Returns true if reset is enabled for any settings type.
  bool ShouldPromptForReset();

  std::string homepage() const;
  ResetState homepage_reset_state() const;

  std::string default_search() const;
  ResetState default_search_reset_state() const;

  // Returns list of all current startup URLs. Returns empty list if session
  // startup is set to show the NTP or restore last session.
  const std::vector<GURL>& startup_urls() const;
  // Returns the list of all startup URLs that have a match in the prompt
  // config. This is a subset of the URLs returned by |startup_urls()|.
  const std::vector<GURL>& startup_urls_to_reset() const;
  ResetState startup_urls_reset_state() const;

  // Returns a map of extension ID -> ExtensionInfo for all extensions that will
  // be disabled.
  const ExtensionMap& extensions_to_disable() const;

 private:
  void InitHomepageData();
  void InitDefaultSearchData();
  void InitExtensionData();
  void InitStartupUrlsData();

  Profile* const profile_;
  std::unique_ptr<SettingsResetPromptConfig> prompt_config_;
  std::unique_ptr<ResettableSettingsSnapshot> settings_snapshot_;

  // Bits to keep track of which settings types have been initialized.
  uint32_t settings_types_initialized_;

  int homepage_reset_domain_id_;
  ResetState homepage_reset_state_;

  int default_search_reset_domain_id_;
  ResetState default_search_reset_state_;

  std::vector<GURL> startup_urls_;
  std::vector<GURL> startup_urls_to_reset_;
  // Reset domain IDs for URLs in |startup_urls_to_reset_|;
  std::unordered_set<int> domain_ids_for_startup_urls_to_reset_;
  ResetState startup_urls_reset_state_;

  ExtensionMap extensions_to_disable_;

  DISALLOW_COPY_AND_ASSIGN(SettingsResetPromptModel);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_MODEL_H_
