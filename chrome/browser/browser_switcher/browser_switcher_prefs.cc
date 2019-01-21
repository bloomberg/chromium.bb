// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace browser_switcher {

RuleSet::RuleSet() = default;
RuleSet::~RuleSet() = default;

BrowserSwitcherPrefs::BrowserSwitcherPrefs(PrefService* prefs) : prefs_(prefs) {
  DCHECK(prefs_);

  change_registrar_.Init(prefs_);

  const struct {
    const char* pref_name;
    void (BrowserSwitcherPrefs::*callback)();
  } hooks[] = {
      {prefs::kAlternativeBrowserPath,
       &BrowserSwitcherPrefs::AlternativeBrowserPathChanged},
      {prefs::kAlternativeBrowserParameters,
       &BrowserSwitcherPrefs::AlternativeBrowserParametersChanged},
      {prefs::kUrlList, &BrowserSwitcherPrefs::UrlListChanged},
      {prefs::kUrlGreylist, &BrowserSwitcherPrefs::GreylistChanged},
  };

  // Listen for pref changes, and run all the hooks once to initialize state.
  for (const auto& hook : hooks) {
    change_registrar_.Add(
        hook.pref_name,
        base::BindRepeating(hook.callback, base::Unretained(this)));
    (*this.*hook.callback)();
  }
}

BrowserSwitcherPrefs::~BrowserSwitcherPrefs() = default;

// static
void BrowserSwitcherPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kEnabled, false);
  registry->RegisterStringPref(prefs::kAlternativeBrowserPath, "");
  registry->RegisterListPref(prefs::kAlternativeBrowserParameters);
  registry->RegisterListPref(prefs::kUrlList);
  registry->RegisterListPref(prefs::kUrlGreylist);
  registry->RegisterStringPref(prefs::kExternalSitelistUrl, "");
#if defined(OS_WIN)
  registry->RegisterBooleanPref(prefs::kUseIeSitelist, false);
#endif
}

bool BrowserSwitcherPrefs::IsEnabled() const {
  return prefs_->GetBoolean(prefs::kEnabled) &&
         prefs_->IsManagedPreference(prefs::kEnabled);
}

const std::string& BrowserSwitcherPrefs::GetAlternativeBrowserPath() const {
  return alt_browser_path_;
}

const std::vector<std::string>&
BrowserSwitcherPrefs::GetAlternativeBrowserParameters() const {
  return alt_browser_params_;
}

const RuleSet& BrowserSwitcherPrefs::GetRules() const {
  return rules_;
}

GURL BrowserSwitcherPrefs::GetExternalSitelistUrl() const {
  if (!prefs_->IsManagedPreference(prefs::kExternalSitelistUrl))
    return GURL();
  return GURL(prefs_->GetString(prefs::kExternalSitelistUrl));
}

#if defined(OS_WIN)
bool BrowserSwitcherPrefs::UseIeSitelist() const {
  if (!prefs_->IsManagedPreference(prefs::kUseIeSitelist))
    return false;
  return prefs_->GetBoolean(prefs::kUseIeSitelist);
}
#endif

void BrowserSwitcherPrefs::AlternativeBrowserPathChanged() {
  alt_browser_path_.clear();
  if (prefs_->IsManagedPreference(prefs::kAlternativeBrowserPath))
    alt_browser_path_ = prefs_->GetString(prefs::kAlternativeBrowserPath);
}

void BrowserSwitcherPrefs::AlternativeBrowserParametersChanged() {
  alt_browser_params_.clear();
  if (!prefs_->IsManagedPreference(prefs::kAlternativeBrowserParameters))
    return;
  const base::ListValue* params =
      prefs_->GetList(prefs::kAlternativeBrowserParameters);
  for (const auto& param : *params) {
    std::string param_string = param.GetString();
    alt_browser_params_.push_back(param_string);
  }
}

void BrowserSwitcherPrefs::UrlListChanged() {
  rules_.sitelist.clear();

  if (!prefs_->IsManagedPreference(prefs::kUrlList))
    return;

  UMA_HISTOGRAM_COUNTS_100000(
      "BrowserSwitcher.UrlListSize",
      prefs_->GetList(prefs::kUrlList)->GetList().size());

  bool has_wildcard = false;
  for (const auto& url : *prefs_->GetList(prefs::kUrlList)) {
    rules_.sitelist.push_back(url.GetString());
    if (url.GetString() == "*")
      has_wildcard = true;
  }

  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.UrlListWildcard", has_wildcard);
}

void BrowserSwitcherPrefs::GreylistChanged() {
  rules_.greylist.clear();

  // This pref is sensitive. Only set through policies.
  if (!prefs_->IsManagedPreference(prefs::kUrlGreylist))
    return;

  UMA_HISTOGRAM_COUNTS_100000(
      "BrowserSwitcher.GreylistSize",
      prefs_->GetList(prefs::kUrlGreylist)->GetList().size());

  bool has_wildcard = false;
  for (const auto& url : *prefs_->GetList(prefs::kUrlGreylist)) {
    rules_.greylist.push_back(url.GetString());
    if (url.GetString() == "*")
      has_wildcard = true;
  }

  UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.UrlListWildcard", has_wildcard);
}

namespace prefs {

// Path to the executable of the alternative browser, or one of "${chrome}",
// "${ie}", "${firefox}", "${opera}", "${safari}".
const char kAlternativeBrowserPath[] =
    "browser_switcher.alternative_browser_path";

// Arguments to pass to the alternative browser when invoking it via
// |ShellExecute()|.
const char kAlternativeBrowserParameters[] =
    "browser_switcher.alternative_browser_parameters";

// List of host domain names to be opened in an alternative browser.
const char kUrlList[] = "browser_switcher.url_list";

// List of hosts that should not trigger a transition in either browser.
const char kUrlGreylist[] = "browser_switcher.url_greylist";

// URL with an external XML sitelist file to load.
const char kExternalSitelistUrl[] = "browser_switcher.external_sitelist_url";

#if defined(OS_WIN)
// If set to true, use the IE Enterprise Mode Sitelist policy.
const char kUseIeSitelist[] = "browser_switcher.use_ie_sitelist";
#endif

// Disable browser_switcher unless this is set to true.
const char kEnabled[] = "browser_switcher.enabled";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
}

}  // namespace prefs
}  // namespace browser_switcher
