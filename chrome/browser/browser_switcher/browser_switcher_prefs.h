// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_PREFS_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_PREFS_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "url/gurl.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

namespace browser_switcher {

// A named pair type.
struct RuleSet {
  RuleSet();
  ~RuleSet();

  std::vector<std::string> sitelist;
  std::vector<std::string> greylist;
};

// Contains the current state of the prefs related to LBS. For sensitive prefs,
// only respects managed prefs. Also does some type conversions and
// transformations on the prefs (e.g. expanding preset values for
// AlternativeBrowserPath).
class BrowserSwitcherPrefs {
 public:
  explicit BrowserSwitcherPrefs(PrefService* prefs);
  virtual ~BrowserSwitcherPrefs();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns true if the BrowserSwitcher feature is enabled via policy.
  bool IsEnabled() const;

  // Returns the path to the alternative browser to launch, before
  // substitutions. If the pref is not managed, returns the empty string.
  const std::string& GetAlternativeBrowserPath() const;

  // Returns the arguments to pass to the alternative browser, before
  // substitutions. If the pref is not managed, returns the empty string.
  const std::vector<std::string>& GetAlternativeBrowserParameters() const;

  // Returns the sitelist + greylist configured directly through Chrome
  // policies. If the pref is not managed, returns an empty vector.
  const RuleSet& GetRules() const;

  // Returns the URL to download for an external XML sitelist. If the pref is
  // not managed, returns an invalid URL.
  GURL GetExternalSitelistUrl() const;

#if defined(OS_WIN)
  // Returns true if Chrome should download and apply the XML sitelist from
  // IEEM's SiteList policy. If the pref is not managed, returns false.
  bool UseIeSitelist() const;
#endif

 private:
  // Hooks for PrefChangeRegistrar.
  void AlternativeBrowserPathChanged();
  void AlternativeBrowserParametersChanged();
  void UrlListChanged();
  void GreylistChanged();

  PrefService* prefs_;
  PrefChangeRegistrar change_registrar_;

  // Type-converted and/or expanded pref values, updated by the
  // PrefChangeRegistrar hooks.
  std::string alt_browser_path_;
  std::vector<std::string> alt_browser_params_;

  RuleSet rules_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserSwitcherPrefs);
};

namespace prefs {

extern const char kEnabled[];
extern const char kAlternativeBrowserPath[];
extern const char kAlternativeBrowserParameters[];
extern const char kUrlList[];
extern const char kUrlGreylist[];
extern const char kExternalSitelistUrl[];

#if defined(OS_WIN)
extern const char kUseIeSitelist[];
#endif

}  // namespace prefs

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_PREFS_H_
