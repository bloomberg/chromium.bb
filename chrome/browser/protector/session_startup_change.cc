// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/protector/base_setting_change.h"
#include "chrome/browser/protector/histograms.h"
#include "chrome/browser/protector/protector_service.h"
#include "chrome/browser/protector/protector_service_factory.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace protector {

// STARTUP_SETTING and session startup settings change tracked by Protector.
class SessionStartupChange : public BaseSettingChange {
 public:
  SessionStartupChange(const SessionStartupPref& actual,
                       const SessionStartupPref& backup);

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

 private:
  virtual ~SessionStartupChange();

  SessionStartupPref new_pref_;
  SessionStartupPref backup_pref_;

  DISALLOW_COPY_AND_ASSIGN(SessionStartupChange);
};

SessionStartupChange::SessionStartupChange(const SessionStartupPref& actual,
                                           const SessionStartupPref& backup)
    : new_pref_(actual),
      backup_pref_(backup) {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramStartupSettingsChanged,
      actual.type,
      SessionStartupPref::TYPE_COUNT);
}

SessionStartupChange::~SessionStartupChange() {
}

bool SessionStartupChange::Init(Profile* profile) {
  if (!BaseSettingChange::Init(profile))
    return false;
  SessionStartupPref::SetStartupPref(profile, backup_pref_);
  return true;
}

void SessionStartupChange::Apply(Browser* browser) {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramStartupSettingsApplied,
      new_pref_.type,
      SessionStartupPref::TYPE_COUNT);
  SessionStartupPref::SetStartupPref(profile(), new_pref_);
}

void SessionStartupChange::Discard(Browser* browser) {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramStartupSettingsDiscarded,
      new_pref_.type,
      SessionStartupPref::TYPE_COUNT);
  // Nothing to do here since |backup_pref_| was already made active by Init().
}

void SessionStartupChange::Timeout() {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramStartupSettingsTimeout,
      new_pref_.type,
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
  switch (new_pref_.type) {
    case SessionStartupPref::DEFAULT:
      return l10n_util::GetStringUTF16(IDS_CHANGE_STARTUP_SETTINGS_NTP);

    case SessionStartupPref::LAST:
      return l10n_util::GetStringUTF16(IDS_CHANGE_STARTUP_SETTINGS_RESTORE);

    case SessionStartupPref::URLS:
      if (new_pref_.urls.empty()) {
        return l10n_util::GetStringUTF16(IDS_CHANGE_STARTUP_SETTINGS_NTP);
      } else {
        string16 first_url = UTF8ToUTF16(new_pref_.urls[0].host());
        return l10n_util::GetStringFUTF16(IDS_CHANGE_STARTUP_SETTINGS_URLS,
                                          first_url);
      }

    default:
      NOTREACHED();
      return string16();
  }
}

string16 SessionStartupChange::GetDiscardButtonText() const {
  switch (backup_pref_.type) {
    case SessionStartupPref::DEFAULT:
      return l10n_util::GetStringUTF16(IDS_KEEP_STARTUP_SETTINGS_NTP);

    case SessionStartupPref::LAST:
      return l10n_util::GetStringUTF16(IDS_KEEP_STARTUP_SETTINGS_RESTORE);

    case SessionStartupPref::URLS:
      if (backup_pref_.urls.empty()) {
        return l10n_util::GetStringUTF16(IDS_KEEP_STARTUP_SETTINGS_NTP);
      } else {
        string16 first_url = UTF8ToUTF16(backup_pref_.urls[0].host());
        return l10n_util::GetStringFUTF16(IDS_KEEP_STARTUP_SETTINGS_URLS,
                                          first_url);
      }

    default:
      NOTREACHED();
      return string16();
  }
}

BaseSettingChange* CreateSessionStartupChange(
    const SessionStartupPref& actual,
    const SessionStartupPref& backup) {
  return new SessionStartupChange(actual, backup);
}

}  // namespace protector
