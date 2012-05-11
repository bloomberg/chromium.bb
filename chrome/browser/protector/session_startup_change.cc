// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/protector/base_prefs_change.h"
#include "chrome/browser/protector/histograms.h"
#include "chrome/browser/protector/protector_service.h"
#include "chrome/browser/protector/protector_service_factory.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace protector {

// Session startup settings change tracked by Protector.
class SessionStartupChange : public BasePrefsChange {
 public:
  SessionStartupChange(const SessionStartupPref& actual_startup_pref,
                       const StartupTabs& actual_pinned_tabs,
                       const SessionStartupPref& backup_startup_pref,
                       const StartupTabs& backup_pinned_tabs);

  // BaseSettingChange overrides:
  virtual bool Init(Profile* profile) OVERRIDE;
  virtual void Apply(Browser* browser) OVERRIDE;
  virtual void Discard(Browser* browser) OVERRIDE;
  virtual void Timeout() OVERRIDE;
  virtual int GetBadgeIconID() const OVERRIDE;
  virtual int GetMenuItemIconID() const OVERRIDE;
  virtual int GetBubbleIconID() const OVERRIDE;
  virtual string16 GetBubbleTitle() const OVERRIDE;
  virtual string16 GetBubbleMessage() const OVERRIDE;
  virtual string16 GetApplyButtonText() const OVERRIDE;
  virtual string16 GetDiscardButtonText() const OVERRIDE;
  virtual DisplayName GetApplyDisplayName() const OVERRIDE;
  virtual GURL GetNewSettingURL() const OVERRIDE;

 private:
  virtual ~SessionStartupChange();

  // Returns the first URL that was added to the startup pages. Returns the
  // first startup URL if they haven't changed. Returns an empty URL if there
  // are no startup URLs.
  GURL GetFirstNewURL() const;

  // Opens all tabs in |tabs| and makes them pinned.
  void OpenPinnedTabs(Browser* browser, const StartupTabs& tabs);

  const SessionStartupPref new_startup_pref_;
  const SessionStartupPref backup_startup_pref_;
  const StartupTabs new_pinned_tabs_;
  const StartupTabs backup_pinned_tabs_;

  DISALLOW_COPY_AND_ASSIGN(SessionStartupChange);
};

SessionStartupChange::SessionStartupChange(
    const SessionStartupPref& actual_startup_pref,
    const StartupTabs& actual_pinned_tabs,
    const SessionStartupPref& backup_startup_pref,
    const StartupTabs& backup_pinned_tabs)
    : new_startup_pref_(actual_startup_pref),
      backup_startup_pref_(backup_startup_pref),
      new_pinned_tabs_(actual_pinned_tabs),
      backup_pinned_tabs_(backup_pinned_tabs) {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramStartupSettingsChanged,
      actual_startup_pref.type,
      SessionStartupPref::TYPE_COUNT);
}

SessionStartupChange::~SessionStartupChange() {
}

bool SessionStartupChange::Init(Profile* profile) {
  if (!BasePrefsChange::Init(profile))
    return false;
  SessionStartupPref::SetStartupPref(profile, backup_startup_pref_);
  PinnedTabCodec::WritePinnedTabs(profile, backup_pinned_tabs_);
  DismissOnPrefChange(prefs::kRestoreOnStartup);
  DismissOnPrefChange(prefs::kURLsToRestoreOnStartup);
  DismissOnPrefChange(prefs::kPinnedTabs);
  return true;
}

void SessionStartupChange::Apply(Browser* browser) {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramStartupSettingsApplied,
      new_startup_pref_.type,
      SessionStartupPref::TYPE_COUNT);
  IgnorePrefChanges();
  SessionStartupPref::SetStartupPref(profile(), new_startup_pref_);
  PinnedTabCodec::WritePinnedTabs(profile(), new_pinned_tabs_);
  OpenPinnedTabs(browser, new_pinned_tabs_);
}

void SessionStartupChange::Discard(Browser* browser) {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramStartupSettingsDiscarded,
      new_startup_pref_.type,
      SessionStartupPref::TYPE_COUNT);
  IgnorePrefChanges();
  // Nothing to do here since backup has already been made active by Init().
}

void SessionStartupChange::Timeout() {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramStartupSettingsTimeout,
      new_startup_pref_.type,
      SessionStartupPref::TYPE_COUNT);
}

int SessionStartupChange::GetBadgeIconID() const {
  // Icons are the same for homepage and startup settings.
  return IDR_HOMEPAGE_CHANGE_BADGE;
}

int SessionStartupChange::GetMenuItemIconID() const {
  return IDR_HOMEPAGE_CHANGE_MENU;
}

int SessionStartupChange::GetBubbleIconID() const {
  return IDR_HOMEPAGE_CHANGE_ALERT;
}

string16 SessionStartupChange::GetBubbleTitle() const {
  return l10n_util::GetStringUTF16(IDS_STARTUP_SETTINGS_CHANGE_TITLE);
}

string16 SessionStartupChange::GetBubbleMessage() const {
  return l10n_util::GetStringUTF16(IDS_STARTUP_SETTINGS_CHANGE_BUBBLE_MESSAGE);
}

string16 SessionStartupChange::GetApplyButtonText() const {
  if (new_startup_pref_.type == SessionStartupPref::LAST)
      return l10n_util::GetStringUTF16(IDS_CHANGE_STARTUP_SETTINGS_RESTORE);

  // Display the domain name of the first startup/pinned URL.
  string16 first_domain = UTF8ToUTF16(GetFirstNewURL().host());
  return first_domain.empty() ?
      l10n_util::GetStringUTF16(IDS_CHANGE_STARTUP_SETTINGS_NTP) :
      l10n_util::GetStringFUTF16(IDS_CHANGE_STARTUP_SETTINGS_URLS,
                                 first_domain);
}

string16 SessionStartupChange::GetDiscardButtonText() const {
  return l10n_util::GetStringUTF16(IDS_KEEP_SETTING);
}

BaseSettingChange::DisplayName
SessionStartupChange::GetApplyDisplayName() const {
  string16 first_domain = UTF8ToUTF16(GetFirstNewURL().host());
  return first_domain.empty() ?
      BaseSettingChange::GetApplyDisplayName() :
      DisplayName(kSessionStartupChangeNamePriority, first_domain);
}

GURL SessionStartupChange::GetNewSettingURL() const {
  VLOG(1) << "HP URL: " << GetFirstNewURL().spec();
  return GetFirstNewURL();
}

GURL SessionStartupChange::GetFirstNewURL() const {
  if (new_startup_pref_.type == SessionStartupPref::LAST)
    return GURL();

  std::set<GURL> old_urls;
  if (backup_startup_pref_.type == SessionStartupPref::URLS) {
    old_urls.insert(backup_startup_pref_.urls.begin(),
                    backup_startup_pref_.urls.end());
  }
  for (size_t i = 0; i < backup_pinned_tabs_.size(); ++i)
    old_urls.insert(backup_pinned_tabs_[i].url);

  std::vector<GURL> new_urls;
  if (new_startup_pref_.type == SessionStartupPref::URLS) {
    new_urls.insert(new_urls.end(), new_startup_pref_.urls.begin(),
                    new_startup_pref_.urls.end());
  }
  for (size_t i = 0; i < new_pinned_tabs_.size(); ++i)
    new_urls.push_back(new_pinned_tabs_[i].url);

  if (new_urls.empty())
    return GURL();

  // First try to find a URL in new settings that is not present in backup.
  for (size_t i = 0; i < new_urls.size(); ++i) {
    if (!old_urls.count(new_urls[i]))
      return new_urls[i];
  }
  // Then fallback to the first of the current startup URLs - this means that
  // URLs themselves haven't changed.
  return new_urls[0];
}

void SessionStartupChange::OpenPinnedTabs(Browser* browser,
                                          const StartupTabs& tabs) {
  for (size_t i = 0; i < tabs.size(); ++i) {
    browser::NavigateParams params(browser, tabs[i].url,
                                   content::PAGE_TRANSITION_START_PAGE);
    params.disposition = NEW_BACKGROUND_TAB;
    params.tabstrip_index = -1;
    params.tabstrip_add_types = TabStripModel::ADD_PINNED;
    params.extension_app_id = tabs[i].app_id;
    browser::Navigate(&params);
  }
}

BaseSettingChange* CreateSessionStartupChange(
    const SessionStartupPref& actual_startup_pref,
    const StartupTabs& actual_pinned_tabs,
    const SessionStartupPref& backup_startup_pref,
    const StartupTabs& backup_pinned_tabs) {
  return new SessionStartupChange(actual_startup_pref, actual_pinned_tabs,
                                  backup_startup_pref, backup_pinned_tabs);
}

}  // namespace protector
