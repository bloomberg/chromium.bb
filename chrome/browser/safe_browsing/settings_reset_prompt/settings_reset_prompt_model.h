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

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/extension_info.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_prefs_manager.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

class BrandcodedDefaultSettings;
class Profile;
class ProfileResetter;
class ResettableSettingsSnapshot;

namespace safe_browsing {

class SettingsResetPromptConfig;

// Encapsulates information about settings needed for the settings reset prompt
// and implements the reset logic.
//
// When more than one setting has a match in the list of domains in the config
// object, the model chooses one of the settings for reset based on the
// following priority list:
// 1. Default search engine
// 2. Startup URLs
// 3. Homepage
class SettingsResetPromptModel {
 public:
  enum ResetState {
    RESET_REQUIRED = 1,
    NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED = 2,
    NO_RESET_REQUIRED_DUE_TO_ALREADY_PROMPTED_FOR_SETTING = 3,
    NO_RESET_REQUIRED_DUE_TO_RECENTLY_PROMPTED = 4,
    NO_RESET_REQUIRED_DUE_TO_OTHER_SETTING_REQUIRING_RESET = 5,
    NO_RESET_REQUIRED_DUE_TO_POLICY = 6,
    RESET_STATE_MAX = 7
  };

  using ExtensionMap =
      std::unordered_map<extensions::ExtensionId, ExtensionInfo>;
  using CreateCallback =
      base::Callback<void(std::unique_ptr<SettingsResetPromptModel>)>;

  // Creates a new |SettingsResetPromptModel| and passes it to |callback|. This
  // function should be called on the UI thread.
  static void Create(Profile* profile,
                     std::unique_ptr<SettingsResetPromptConfig> prompt_config,
                     CreateCallback callback);
  static std::unique_ptr<SettingsResetPromptModel> CreateForTesting(
      Profile* profile,
      std::unique_ptr<SettingsResetPromptConfig> prompt_config,
      std::unique_ptr<ResettableSettingsSnapshot> settings_snapshot,
      std::unique_ptr<BrandcodedDefaultSettings> default_settings,
      std::unique_ptr<ProfileResetter> profile_resetter);

  virtual ~SettingsResetPromptModel();

  Profile* profile() const;
  SettingsResetPromptConfig* config() const;

  // Returns true if reset is enabled for any settings type.
  virtual bool ShouldPromptForReset() const;
  // Resets the settings whose reset states are set to |RESET_REQUIRED| as
  // returned by the methods below. Should be called only on the UI
  // thread. |done_callback| will called from the UI thread when the reset
  // operation has been completed.
  //
  // NOTE: Can only be called once during the lifetime of this object.
  virtual void PerformReset(const base::Closure& done_callback);
  // To be called when the reset prompt dialog has been shown so that
  // preferences can be updated.
  virtual void DialogShown();

  virtual GURL homepage() const;
  virtual ResetState homepage_reset_state() const;

  virtual GURL default_search() const;
  virtual ResetState default_search_reset_state() const;

  // Returns list of all current startup URLs. Returns empty list if session
  // startup is set to show the NTP or restore last session.
  virtual const std::vector<GURL>& startup_urls() const;
  // Returns the list of all startup URLs that have a match in the prompt
  // config. This is a subset of the URLs returned by |startup_urls()|.
  virtual const std::vector<GURL>& startup_urls_to_reset() const;
  virtual ResetState startup_urls_reset_state() const;

  // Returns a map of extension ID -> ExtensionInfo for all extensions that will
  // be disabled.
  virtual const ExtensionMap& extensions_to_disable() const;

  void ReportUmaMetrics() const;

 protected:
  // Exposed for mocking in tests.
  SettingsResetPromptModel(
      Profile* profile,
      std::unique_ptr<SettingsResetPromptConfig> prompt_config,
      std::unique_ptr<ResettableSettingsSnapshot> settings_snapshot,
      std::unique_ptr<BrandcodedDefaultSettings> default_settings,
      std::unique_ptr<ProfileResetter> profile_resetter);

 private:
  static void OnSettingsFetched(
      Profile* profile,
      std::unique_ptr<SettingsResetPromptConfig> prompt_config,
      SettingsResetPromptModel::CreateCallback callback,
      std::unique_ptr<BrandcodedDefaultSettings> default_settings);

  // Functions to be called by the constructor to initialize the model
  // object. These functions should be called in the order in which they are
  // declared here so that the correct setting is chosen for the prompt when
  // more than one setting requires reset (see also the class description).
  void InitDefaultSearchData();
  void InitStartupUrlsData();
  void InitHomepageData();
  void InitExtensionData();

  // Helper function for the Init* functions above to determine the reset state
  // of settings that have a match in the config.
  ResetState GetResetStateForSetting(
      const base::Time& last_triggered_for_setting) const;

  // Return true if any setting's reset state is set to |RESET_REQUIRED|.
  bool SomeSettingRequiresReset() const;

  bool SomeSettingIsManaged() const;
  bool SomeExtensionMustRemainEnabled() const;

  Profile* const profile_;

  SettingsResetPromptPrefsManager prefs_manager_;
  std::unique_ptr<SettingsResetPromptConfig> prompt_config_;
  std::unique_ptr<ResettableSettingsSnapshot> settings_snapshot_;
  // |default_settings_| should only be accessed on the UI thread after
  // construction.
  std::unique_ptr<BrandcodedDefaultSettings> default_settings_;
  std::unique_ptr<ProfileResetter> profile_resetter_;

  // A single timestamp to be used by all initialization functions to determine
  // if enough time has passed between the last time the prompt was shown and
  // "now" for a new prompt to be shown.
  base::Time now_;
  // The time since the last prompt was shown for any setting.
  base::TimeDelta time_since_last_prompt_;

  // Bits to keep track of which settings types have been initialized.
  uint32_t settings_types_initialized_;

  GURL homepage_url_;
  int homepage_reset_domain_id_;
  ResetState homepage_reset_state_;

  GURL default_search_url_;
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
