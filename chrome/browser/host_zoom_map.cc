// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/host_zoom_map.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

HostZoomMap::HostZoomMap(Profile* profile) : profile_(profile) {
  const DictionaryValue* host_zoom_dictionary =
      profile_->GetPrefs()->GetDictionary(prefs::kPerHostZoomLevels);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (host_zoom_dictionary != NULL) {
    for (DictionaryValue::key_iterator i(host_zoom_dictionary->begin_keys());
         i != host_zoom_dictionary->end_keys(); ++i) {
      std::wstring wide_host(*i);
      int zoom_level = 0;
      bool success = host_zoom_dictionary->GetIntegerWithoutPathExpansion(
          wide_host, &zoom_level);
      DCHECK(success);
      host_zoom_levels_[WideToUTF8(wide_host)] = zoom_level;
    }
  }
}

// static
void HostZoomMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kPerHostZoomLevels);
}

int HostZoomMap::GetZoomLevel(const std::string& host) const {
  AutoLock auto_lock(lock_);
  HostZoomLevels::const_iterator i(host_zoom_levels_.find(host));
  return (i == host_zoom_levels_.end()) ? 0 : i->second;
}

void HostZoomMap::SetZoomLevel(const std::string& host, int level) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (host.empty())
    return;

  {
    AutoLock auto_lock(lock_);
    if (level == 0)
      host_zoom_levels_.erase(host);
    else
      host_zoom_levels_[host] = level;
  }

  DictionaryValue* host_zoom_dictionary =
      profile_->GetPrefs()->GetMutableDictionary(prefs::kPerHostZoomLevels);
  std::wstring wide_host(UTF8ToWide(host));
  if (level == 0) {
    host_zoom_dictionary->RemoveWithoutPathExpansion(wide_host, NULL);
  } else {
    host_zoom_dictionary->SetWithoutPathExpansion(wide_host,
        Value::CreateIntegerValue(level));
  }
}

void HostZoomMap::ResetToDefaults() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  {
    AutoLock auto_lock(lock_);
    host_zoom_levels_.clear();
  }

  profile_->GetPrefs()->ClearPref(prefs::kPerHostZoomLevels);
}

HostZoomMap::~HostZoomMap() {
}
