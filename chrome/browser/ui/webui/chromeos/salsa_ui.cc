// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/salsa_ui.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

// Whitelist of which preferences are possible targets for Salsa treatments.
// If new preferences are added and they are to be used in an experiment, then
// they must be added to this list as well to keep chrome://salsa from
// changing arbitrary values.

namespace {

const char* kWhitelist[] = {
  prefs::kMaxSeparationForGestureTouchesInPixels,
  prefs::kFlingAccelerationCurveCoefficient0,
  prefs::kFlingAccelerationCurveCoefficient1,
  prefs::kFlingAccelerationCurveCoefficient2,
  prefs::kFlingAccelerationCurveCoefficient3,
  prefs::kTabScrubActivationDelayInMS,
  prefs::kOverscrollHorizontalThresholdComplete,
  prefs::kOverscrollVerticalThresholdComplete,
  prefs::kOverscrollMinimumThresholdStart,
  prefs::kOverscrollVerticalThresholdStart,
  prefs::kOverscrollHorizontalResistThreshold,
  prefs::kOverscrollVerticalResistThreshold,
  prefs::kFlingCurveTouchscreenAlpha,
  prefs::kFlingCurveTouchscreenBeta,
  prefs::kFlingCurveTouchscreenGamma,
  prefs::kFlingCurveTouchpadAlpha,
  prefs::kFlingCurveTouchpadBeta,
  prefs::kFlingCurveTouchpadGamma,
};

void RevertPreferences(PrefService* prefs,
                       std::map<int, const base::Value*>* vals) {
  std::map<int, const base::Value*>::const_iterator it;
  for (it = vals->begin(); it != vals->end(); ++it) {
    if (!prefs->FindPreference(kWhitelist[it->first]))
      continue;

    if (!it->second) {
      prefs->ClearPref(kWhitelist[it->first]);
    } else {
      prefs->Set(kWhitelist[it->first], *it->second);
      delete it->second;
    }
  }
}

} // namespace

SalsaUI::SalsaUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://salsa source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUISalsaHost);

  // Register callback handlers.
  web_ui->RegisterMessageCallback(
      "salsaSetPreferenceValue",
      base::Bind(&SalsaUI::SetPreferenceValue,
                 base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "salsaBackupPreferenceValue",
      base::Bind(&SalsaUI::BackupPreferenceValue,
                 base::Unretained(this)));

  // Add required resources.
  html_source->AddResourcePath("salsa.css", IDR_SALSA_CSS);
  html_source->AddResourcePath("salsa.js", IDR_SALSA_JS);
  html_source->SetDefaultResource(IDR_SALSA_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

SalsaUI::~SalsaUI() {
  std::map<int, const base::Value*>* values_to_revert =
      new std::map<int, const base::Value*>;
  values_to_revert->swap(orig_values_);

  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&RevertPreferences, prefs, base::Owned(values_to_revert))
    );
}

int SalsaUI::WhitelistIndex(const char* key) const {
  if (!key)
    return -1;

  int len = arraysize(kWhitelist);
  for (int i = 0; i < len; ++i) {
    if (!strcmp(key, kWhitelist[i]))
      return i;
  }
  return -1;
}

void SalsaUI::SetPreferenceValue(const base::ListValue* args) {
  std::string pref_name;
  const base::Value* value;
  if (!args->GetString(0, &pref_name) || !args->Get(1, &value))
    return;

  int index = WhitelistIndex(pref_name.c_str());
  if (index < 0)
    return;

  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();
  const PrefService::Preference* pref =
      prefs->FindPreference(kWhitelist[index]);

  if (pref->GetType() == value->GetType()) {
    prefs->Set(kWhitelist[index], *value);
  } else if (pref->GetType() == base::Value::TYPE_DOUBLE &&
             value->GetType() == base::Value::TYPE_INTEGER) {
    int int_val;
    if (!value->GetAsInteger(&int_val))
      return;
    base::FundamentalValue double_val(static_cast<double>(int_val));
    prefs->Set(kWhitelist[index], double_val);
  }
}

void SalsaUI::BackupPreferenceValue(const base::ListValue* args) {
  std::string pref_name;
  if (!args->GetString(0, &pref_name))
    return;

  int index = WhitelistIndex(pref_name.c_str());
  if (index < 0)
    return;

  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();
  const PrefService::Preference* pref =
      prefs->FindPreference(kWhitelist[index]);

  if (!pref)
    return;

  // Get our own copy of the user defined value or NULL if they are using the
  // default. You have to make a copy since they'll be used in the destructor
  // to restore the values and we need to make sure they're still around.
  orig_values_[index] =
      pref->IsDefaultValue() ? NULL : pref->GetValue()->DeepCopy();
}
