// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/protector/base_prefs_change.h"
#include "chrome/browser/protector/histograms.h"
#include "chrome/browser/protector/protector_service.h"
#include "chrome/browser/protector/protector_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"

namespace protector {

// Unknown change to Preferences with invalid backup.
class PrefsBackupInvalidChange : public BasePrefsChange {
 public:
  PrefsBackupInvalidChange();

  // BasePrefsChange overrides:
  virtual bool Init(Profile* profile) OVERRIDE;
  virtual void InitWhenDisabled(Profile* profile) OVERRIDE;
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
  virtual bool CanBeMerged() const OVERRIDE;

 private:
  virtual ~PrefsBackupInvalidChange();

  // Applies default settings values when appropriate.
  void ApplyDefaults(Profile* profile);

  // True if session startup prefs have been reset.
  bool startup_pref_reset_;

  DISALLOW_COPY_AND_ASSIGN(PrefsBackupInvalidChange);
};

PrefsBackupInvalidChange::PrefsBackupInvalidChange()
    : startup_pref_reset_(false) {
}

PrefsBackupInvalidChange::~PrefsBackupInvalidChange() {
}

bool PrefsBackupInvalidChange::Init(Profile* profile) {
  if (!BasePrefsChange::Init(profile))
    return false;
  ApplyDefaults(profile);
  DismissOnPrefChange(prefs::kHomePageIsNewTabPage);
  DismissOnPrefChange(prefs::kHomePage);
  DismissOnPrefChange(prefs::kShowHomeButton);
  DismissOnPrefChange(prefs::kRestoreOnStartup);
  DismissOnPrefChange(prefs::kURLsToRestoreOnStartup);
  DismissOnPrefChange(prefs::kPinnedTabs);
  return true;
}

void PrefsBackupInvalidChange::InitWhenDisabled(Profile* profile) {
  // Nothing to do here since the backup has been already reset.
}

void PrefsBackupInvalidChange::Apply(Browser* browser) {
  NOTREACHED();
}

void PrefsBackupInvalidChange::Discard(Browser* browser) {
  // TODO(ivankr): highlight the protected prefs on the settings page
  // (http://crbug.com/119088).
  ProtectorServiceFactory::GetForProfile(profile())->OpenTab(
      GURL(chrome::kChromeUISettingsURL), browser);
}

void PrefsBackupInvalidChange::Timeout() {
}

int PrefsBackupInvalidChange::GetBadgeIconID() const {
  return IDR_UPDATE_BADGE4;
}

int PrefsBackupInvalidChange::GetMenuItemIconID() const {
  return IDR_UPDATE_MENU4;
}

int PrefsBackupInvalidChange::GetBubbleIconID() const {
  return IDR_INPUT_ALERT;
}

string16 PrefsBackupInvalidChange::GetBubbleTitle() const {
  return l10n_util::GetStringUTF16(IDS_SETTING_CHANGE_TITLE);
}

string16 PrefsBackupInvalidChange::GetBubbleMessage() const {
  return startup_pref_reset_ ?
      l10n_util::GetStringUTF16(
          IDS_SETTING_CHANGE_NO_BACKUP_STARTUP_RESET_BUBBLE_MESSAGE) :
      l10n_util::GetStringUTF16(IDS_SETTING_CHANGE_BUBBLE_MESSAGE);
}

string16 PrefsBackupInvalidChange::GetApplyButtonText() const {
  // Don't show this button.
  return string16();
}

string16 PrefsBackupInvalidChange::GetDiscardButtonText() const {
  return l10n_util::GetStringUTF16(IDS_EDIT_SETTINGS);
}

void PrefsBackupInvalidChange::ApplyDefaults(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  SessionStartupPref startup_pref = SessionStartupPref::GetStartupPref(prefs);
  if (startup_pref.type != SessionStartupPref::LAST) {
    // If startup type is LAST, resetting it is dangerous (the whole previous
    // session will be lost).
    prefs->ClearPref(prefs::kRestoreOnStartup);
    startup_pref_reset_ = true;
  }
  prefs->ClearPref(prefs::kHomePageIsNewTabPage);
  prefs->ClearPref(prefs::kHomePage);
  prefs->ClearPref(prefs::kShowHomeButton);
}

bool PrefsBackupInvalidChange::CanBeMerged() const {
  return false;
}

BaseSettingChange* CreatePrefsBackupInvalidChange() {
  return new PrefsBackupInvalidChange();
}

}  // namespace protector
