// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/host_zoom_map.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/scoped_pref_update.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

HostZoomMap::HostZoomMap(Profile* profile)
    : profile_(profile),
      updating_preferences_(false) {
  Load();
  registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                 Source<Profile>(profile));
  // Don't observe pref changes (e.g. from sync) in Incognito; once we create
  // the incognito window it should have no further connection to the main
  // profile/prefs.
  if (!profile_->IsOffTheRecord())
    profile_->GetPrefs()->AddPrefObserver(prefs::kPerHostZoomLevels, this);
}

void HostZoomMap::Load() {
  if (!profile_)
    return;

  AutoLock auto_lock(lock_);
  host_zoom_levels_.clear();
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

int HostZoomMap::GetZoomLevel(const GURL& url) const {
  std::string host(net::GetHostOrSpecFromURL(url));
  AutoLock auto_lock(lock_);
  HostZoomLevels::const_iterator i(host_zoom_levels_.find(host));
  return (i == host_zoom_levels_.end()) ? 0 : i->second;
}

void HostZoomMap::SetZoomLevel(const GURL& url, int level) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (!profile_)
    return;

  std::string host(net::GetHostOrSpecFromURL(url));

  {
    AutoLock auto_lock(lock_);
    if (level == 0)
      host_zoom_levels_.erase(host);
    else
      host_zoom_levels_[host] = level;
  }

  Details<std::string> details(&host);
  NotificationService::current()->Notify(NotificationType::ZOOM_LEVEL_CHANGED,
                                         Source<Profile>(profile_),
                                         details);

  // If we're in incognito mode, don't persist changes to the prefs.  We'll keep
  // them in memory only so they will be forgotten on exiting incognito.
  if (profile_->IsOffTheRecord())
    return;

  updating_preferences_ = true;
  {
    ScopedPrefUpdate update(profile_->GetPrefs(), prefs::kPerHostZoomLevels);
    DictionaryValue* host_zoom_dictionary =
        profile_->GetPrefs()->GetMutableDictionary(prefs::kPerHostZoomLevels);
    std::wstring wide_host(UTF8ToWide(host));
    if (level == 0) {
      host_zoom_dictionary->RemoveWithoutPathExpansion(wide_host, NULL);
    } else {
      host_zoom_dictionary->SetWithoutPathExpansion(
          wide_host,
          Value::CreateIntegerValue(level));
    }
  }
  updating_preferences_ = false;
}

void HostZoomMap::ResetToDefaults() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (!profile_)
    return;

  {
    AutoLock auto_lock(lock_);
    host_zoom_levels_.clear();
  }

  updating_preferences_ = true;
  profile_->GetPrefs()->ClearPref(prefs::kPerHostZoomLevels);
  updating_preferences_ = false;
}

void HostZoomMap::Shutdown() {
  if (!profile_)
    return;

  registrar_.Remove(this,
                    NotificationType::PROFILE_DESTROYED,
                    Source<Profile>(profile_));
  if (!profile_->IsOffTheRecord())
    profile_->GetPrefs()->RemovePrefObserver(prefs::kPerHostZoomLevels, this);
  profile_ = NULL;
}

void HostZoomMap::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // If the profile is going away, we need to stop using it.
  if (type == NotificationType::PROFILE_DESTROYED) {
    Shutdown();
    return;
  }

  if (type == NotificationType::PREF_CHANGED) {
    // If we are updating our own preference, don't reload.
    if (updating_preferences_)
      return;

    std::wstring* name = Details<std::wstring>(details).ptr();
    if (prefs::kPerHostZoomLevels == *name) {
      Load();
      return;
    }
  }

  NOTREACHED() << "Unexpected preference observed.";
}

HostZoomMap::~HostZoomMap() {
  Shutdown();
}
