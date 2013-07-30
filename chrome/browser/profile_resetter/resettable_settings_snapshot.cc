// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"

ResettableSettingsSnapshot::ResettableSettingsSnapshot(Profile* profile)
    : startup_(SessionStartupPref::GetStartupPref(profile)) {
  // URLs are always stored sorted.
  std::sort(startup_.urls.begin(), startup_.urls.end());

  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs);
  homepage_ = prefs->GetString(prefs::kHomePage);
  homepage_is_ntp_ = prefs->GetBoolean(prefs::kHomePageIsNewTabPage);

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(service);
  TemplateURL* dse = service->GetDefaultSearchProvider();
  if (dse)
    dse_url_ = dse->url();
}

ResettableSettingsSnapshot::~ResettableSettingsSnapshot() {}

void ResettableSettingsSnapshot::SubtractStartupURLs(
    const ResettableSettingsSnapshot& snapshot) {
  std::vector<GURL> urls;
  std::set_difference(startup_.urls.begin(), startup_.urls.end(),
                      snapshot.startup_.urls.begin(),
                      snapshot.startup_.urls.end(),
                      std::back_inserter(urls));
  startup_.urls.swap(urls);
}

int ResettableSettingsSnapshot::FindDifferentFields(
    const ResettableSettingsSnapshot& snapshot) const {
  int bit_mask = 0;

  if (startup_.urls != snapshot.startup_.urls) {
    bit_mask |= STARTUP_URLS;
  }

  if (startup_.type != snapshot.startup_.type)
    bit_mask |= STARTUP_TYPE;

  if (homepage_ != snapshot.homepage_)
    bit_mask |= HOMEPAGE;

  if (homepage_is_ntp_ != snapshot.homepage_is_ntp_)
    bit_mask |= HOMEPAGE_IS_NTP;

  if (dse_url_ != snapshot.dse_url_)
    bit_mask |= DSE_URL;

  return bit_mask;
}
