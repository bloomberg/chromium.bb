// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/protector/base_prefs_change.h"
#include "chrome/browser/protector/histograms.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace protector {

// Homepage change tracked by Protector.
class HomepageChange : public BasePrefsChange {
 public:
  // Enum for reporting UMA statistics.
  enum HomepageType {
    HOMEPAGE_NTP = 0,
    HOMEPAGE_URL,

    // Must be the last value
    HOMEPAGE_TYPE_COUNT
  };

  HomepageChange(const std::string& actual_homepage,
                 bool actual_homepage_is_ntp,
                 bool actual_show_homepage,
                 const std::string& backup_homepage,
                 bool backup_homepage_is_ntp,
                 bool backup_show_homepage);

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
  virtual ~HomepageChange();

  const std::string new_homepage_;
  const std::string backup_homepage_;
  const bool new_homepage_is_ntp_;
  const bool backup_homepage_is_ntp_;
  const bool new_show_homepage_;
  const bool backup_show_homepage_;
  const HomepageType new_homepage_type_;

  DISALLOW_COPY_AND_ASSIGN(HomepageChange);
};

HomepageChange::HomepageChange(
    const std::string& actual_homepage,
    bool actual_homepage_is_ntp,
    bool actual_show_homepage,
    const std::string& backup_homepage,
    bool backup_homepage_is_ntp,
    bool backup_show_homepage)
    : new_homepage_(actual_homepage),
      backup_homepage_(backup_homepage),
      new_homepage_is_ntp_(actual_homepage_is_ntp),
      backup_homepage_is_ntp_(backup_homepage_is_ntp),
      new_show_homepage_(actual_show_homepage),
      backup_show_homepage_(backup_show_homepage),
      new_homepage_type_(actual_homepage_is_ntp ? HOMEPAGE_NTP : HOMEPAGE_URL) {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramHomepageChanged,
      new_homepage_type_,
      HOMEPAGE_TYPE_COUNT);
}

HomepageChange::~HomepageChange() {
}

bool HomepageChange::Init(Profile* profile) {
  if (!BasePrefsChange::Init(profile))
    return false;
  PrefService* prefs = profile->GetPrefs();
  prefs->SetString(prefs::kHomePage, backup_homepage_);
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, backup_homepage_is_ntp_);
  prefs->SetBoolean(prefs::kShowHomeButton, backup_show_homepage_);
  DismissOnPrefChange(prefs::kHomePage);
  DismissOnPrefChange(prefs::kHomePageIsNewTabPage);
  DismissOnPrefChange(prefs::kShowHomeButton);
  return true;
}

void HomepageChange::Apply(Browser* browser) {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramHomepageApplied,
      new_homepage_type_,
      HOMEPAGE_TYPE_COUNT);
  IgnorePrefChanges();
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetString(prefs::kHomePage, new_homepage_);
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, new_homepage_is_ntp_);
  prefs->SetBoolean(prefs::kShowHomeButton, new_show_homepage_);
}

void HomepageChange::Discard(Browser* browser) {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramHomepageDiscarded,
      new_homepage_type_,
      HOMEPAGE_TYPE_COUNT);
  IgnorePrefChanges();
  // Nothing to do here since backup has already been made active by Init().
}

void HomepageChange::Timeout() {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramHomepageTimeout,
      new_homepage_type_,
      HOMEPAGE_TYPE_COUNT);
}

int HomepageChange::GetBadgeIconID() const {
  // Icons are the same for homepage and startup settings.
  return IDR_HOMEPAGE_CHANGE_BADGE;
}

int HomepageChange::GetMenuItemIconID() const {
  return IDR_HOMEPAGE_CHANGE_MENU;
}

int HomepageChange::GetBubbleIconID() const {
  return IDR_HOMEPAGE_CHANGE_ALERT;
}

string16 HomepageChange::GetBubbleTitle() const {
  return l10n_util::GetStringUTF16(IDS_HOMEPAGE_CHANGE_TITLE);
}

string16 HomepageChange::GetBubbleMessage() const {
  return l10n_util::GetStringUTF16(IDS_HOMEPAGE_CHANGE_BUBBLE_MESSAGE);
}

string16 HomepageChange::GetApplyButtonText() const {
  GURL homepage_url(GetNewSettingURL());
  return homepage_url.is_empty() ?
      l10n_util::GetStringUTF16(IDS_CHANGE_HOMEPAGE_NTP) :
      l10n_util::GetStringFUTF16(IDS_CHANGE_HOMEPAGE,
                                 UTF8ToUTF16(homepage_url.host()));
}

string16 HomepageChange::GetDiscardButtonText() const {
  GURL new_homepage_url(GetNewSettingURL());
  GURL backup_homepage_url;
  if (!backup_homepage_is_ntp_)
    backup_homepage_url = GURL(backup_homepage_);
  if (backup_homepage_url.host() == new_homepage_url.host()) {
    // Display a generic string if new setting looks the same as the backup (for
    // example, when homepage hasn't changed but 'show homepage button' has).
    return l10n_util::GetStringUTF16(IDS_KEEP_SETTING);
  }
  return backup_homepage_url.is_empty() ?
      l10n_util::GetStringUTF16(IDS_KEEP_HOMEPAGE_NTP) :
      l10n_util::GetStringFUTF16(IDS_KEEP_HOMEPAGE,
                                 UTF8ToUTF16(backup_homepage_url.host()));
}

BaseSettingChange::DisplayName HomepageChange::GetApplyDisplayName() const {
  GURL homepage_url(GetNewSettingURL());
  return homepage_url.is_empty() ?
      DisplayName(kDefaultNamePriority, string16()) :
      DisplayName(kHomepageChangeNamePriority,
                  UTF8ToUTF16(homepage_url.host()));
}

GURL HomepageChange::GetNewSettingURL() const {
  return new_homepage_is_ntp_ ? GURL() : GURL(new_homepage_);
}

BaseSettingChange* CreateHomepageChange(
    const std::string& actual_homepage,
    bool actual_homepage_is_ntp,
    bool actual_show_homepage,
    const std::string& backup_homepage,
    bool backup_homepage_is_ntp,
    bool backup_show_homepage) {
  return new HomepageChange(
      actual_homepage, actual_homepage_is_ntp, actual_show_homepage,
      backup_homepage, backup_homepage_is_ntp, backup_show_homepage);
}

}  // namespace protector
