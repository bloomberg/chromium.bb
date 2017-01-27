// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_MODEL_H_
#define CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_MODEL_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/extension_info.h"
#include "extensions/common/extension_id.h"

class Profile;
class ResettableSettingsSnapshot;

namespace safe_browsing {

class SettingsResetPromptConfig;

// Encapsulates information about settings needed for the settings reset prompt
// and implements the reset logic.
class SettingsResetPromptModel {
 public:
  enum ResetState {
    ENABLED = 1,
    DISABLED_DUE_TO_DOMAIN_NOT_MATCHED = 2,
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

  // Returns a map of extension ID -> ExtensionInfo for all extensions that will
  // be disabled.
  const ExtensionMap& extensions_to_disable() const;

 private:
  void InitHomepageData();
  void InitDefaultSearchData();
  void InitExtensionData();

  Profile* const profile_;
  std::unique_ptr<SettingsResetPromptConfig> prompt_config_;
  std::unique_ptr<ResettableSettingsSnapshot> settings_snapshot_;

  int homepage_reset_domain_id_;
  ResetState homepage_reset_state_;

  int default_search_reset_domain_id_;
  ResetState default_search_reset_state_;

  ExtensionMap extensions_to_disable_;

  DISALLOW_COPY_AND_ASSIGN(SettingsResetPromptModel);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_MODEL_H_
