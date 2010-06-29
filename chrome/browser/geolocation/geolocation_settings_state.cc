// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_settings_state.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/pref_names.h"
#include "net/base/net_util.h"

GeolocationSettingsState::GeolocationSettingsState(Profile* profile)
  : profile_(profile) {
}

GeolocationSettingsState::~GeolocationSettingsState() {
}

void GeolocationSettingsState::OnGeolocationPermissionSet(
    const GURL& requesting_origin, bool allowed) {
  state_map_[requesting_origin] =
      allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
}

void GeolocationSettingsState::DidNavigate(
    const NavigationController::LoadCommittedDetails& details) {
  if (details.entry)
    embedder_url_ = details.entry->url();
  if (state_map_.empty())
    return;
  if (!details.entry ||
      details.previous_url.GetOrigin() != details.entry->url().GetOrigin()) {
    state_map_.clear();
    return;
  }
  // We're in the same origin, check if there's any icon to be displayed.
  unsigned int tab_state_flags = 0;
  GetDetailedInfo(NULL, &tab_state_flags);
  if (!(tab_state_flags & TABSTATE_HAS_ANY_ICON))
    state_map_.clear();
}

void GeolocationSettingsState::GetDetailedInfo(
    FormattedHostsPerState* formatted_hosts_per_state,
    unsigned int* tab_state_flags) const {
  DCHECK(tab_state_flags);
  DCHECK(embedder_url_.is_valid());
  const ContentSetting default_setting =
      profile_->GetGeolocationContentSettingsMap()->GetDefaultContentSetting();
  for (StateMap::const_iterator i(state_map_.begin());
       i != state_map_.end(); ++i) {
    if (i->second == CONTENT_SETTING_ALLOW)
      *tab_state_flags |= TABSTATE_HAS_ANY_ALLOWED;
    if (formatted_hosts_per_state) {
      (*formatted_hosts_per_state)[i->second].insert(
          GURLToFormattedHost(i->first));
    }

    const ContentSetting saved_setting =
        profile_->GetGeolocationContentSettingsMap()->GetContentSetting(
            i->first, embedder_url_);
    if (saved_setting != default_setting)
      *tab_state_flags |= TABSTATE_HAS_EXCEPTION;
    if (saved_setting != i->second)
      *tab_state_flags |= TABSTATE_HAS_CHANGED;
    if (saved_setting != CONTENT_SETTING_ASK)
      *tab_state_flags |= TABSTATE_HAS_ANY_ICON;
  }
}

std::string GeolocationSettingsState::GURLToFormattedHost(
    const GURL& url) const {
  std::wstring display_host_wide;
  net::AppendFormattedHost(
      url, UTF8ToWide(profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)),
      &display_host_wide, NULL, NULL);
  return WideToUTF8(display_host_wide);
}
