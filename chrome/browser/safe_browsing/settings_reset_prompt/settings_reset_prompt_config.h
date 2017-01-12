// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_CONFIG_H_
#define CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_CONFIG_H_

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"

class GURL;

namespace safe_browsing {

// Exposed for testing.
extern const base::Feature kSettingsResetPrompt;

// Encapsulates the state of the reset prompt experiment as well as
// associated data.
class SettingsResetPromptConfig {
 public:
  // Returns true if the settings reset prompt study is enabled.
  static bool IsPromptEnabled();
  // Factory method for creating instances of SettingsResetPromptConfig.
  // Returns nullptr if |IsPromptEnabled()| is false or if something is wrong
  // with the config parameters.
  static std::unique_ptr<SettingsResetPromptConfig> Create();

  ~SettingsResetPromptConfig();

  // Returns a non-negative integer ID if |url| should trigger a
  // settings reset prompt and a negative integer otherwise. The IDs
  // identify the domains or entities that we want to prompt the user
  // for and can be used for metrics reporting.
  int UrlToResetDomainId(const GURL& url) const;

  // TODO(alito): parameterize the set of things that we want to reset
  // for so that we can control it from the finch config. For example,
  // with functions like HomepageResetAllowed() etc.
 private:
  typedef std::vector<uint8_t> SHA256Hash;
  struct SHA256HashHasher {
    size_t operator()(const SHA256Hash& key) const;
  };
  enum ConfigError : int;

  SettingsResetPromptConfig();
  bool Init();
  ConfigError ParseDomainHashes(const std::string& domain_hashes_json);

  // Map of 32 byte SHA256 hashes to integer domain IDs.
  std::unordered_map<SHA256Hash, int, SHA256HashHasher> domain_hashes_;

  DISALLOW_COPY_AND_ASSIGN(SettingsResetPromptConfig);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_CONFIG_H_
