// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "content/browser/host_zoom_map.h"

#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebView;

HostZoomMap::HostZoomMap(Profile* profile)
    : profile_(profile),
      updating_preferences_(false) {
  Load();
  default_zoom_level_ =
      profile_->GetPrefs()->GetDouble(prefs::kDefaultZoomLevel);
  registrar_.Add(this, NotificationType::PROFILE_DESTROYED,
                 Source<Profile>(profile));
  // Don't observe pref changes (e.g. from sync) in Incognito; once we create
  // the incognito window it should have no further connection to the main
  // profile/prefs.
  if (!profile_->IsOffTheRecord()) {
    pref_change_registrar_.Init(profile_->GetPrefs());
    pref_change_registrar_.Add(prefs::kPerHostZoomLevels, this);
    pref_change_registrar_.Add(prefs::kDefaultZoomLevel, this);
  }

  registrar_.Add(
      this, NotificationType::RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,
      NotificationService::AllSources());
}

void HostZoomMap::Load() {
  if (!profile_)
    return;

  base::AutoLock auto_lock(lock_);
  host_zoom_levels_.clear();
  const DictionaryValue* host_zoom_dictionary =
      profile_->GetPrefs()->GetDictionary(prefs::kPerHostZoomLevels);
  // Careful: The returned value could be NULL if the pref has never been set.
  if (host_zoom_dictionary != NULL) {
    for (DictionaryValue::key_iterator i(host_zoom_dictionary->begin_keys());
         i != host_zoom_dictionary->end_keys(); ++i) {
      const std::string& host(*i);
      double zoom_level = 0;

      bool success = host_zoom_dictionary->GetDoubleWithoutPathExpansion(
          host, &zoom_level);
      if (!success) {
        // The data used to be stored as ints, so try that.
        int int_zoom_level;
        success = host_zoom_dictionary->GetIntegerWithoutPathExpansion(
            host, &int_zoom_level);
        if (success) {
          zoom_level = static_cast<double>(int_zoom_level);
          // Since the values were once stored as non-clamped, clamp now.
          double zoom_factor = WebView::zoomLevelToZoomFactor(zoom_level);
          if (zoom_factor < WebView::minTextSizeMultiplier) {
            zoom_level =
                WebView::zoomFactorToZoomLevel(WebView::minTextSizeMultiplier);
          } else if (zoom_factor > WebView::maxTextSizeMultiplier) {
            zoom_level =
                WebView::zoomFactorToZoomLevel(WebView::maxTextSizeMultiplier);
          }
        }
      }
      DCHECK(success);
      host_zoom_levels_[host] = zoom_level;
    }
  }
}

// static
void HostZoomMap::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kPerHostZoomLevels);
}

double HostZoomMap::GetZoomLevel(const GURL& url) const {
  std::string host(net::GetHostOrSpecFromURL(url));
  base::AutoLock auto_lock(lock_);
  HostZoomLevels::const_iterator i(host_zoom_levels_.find(host));
  return (i == host_zoom_levels_.end()) ? default_zoom_level_ : i->second;
}

void HostZoomMap::SetZoomLevel(const GURL& url, double level) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!profile_)
    return;

  std::string host(net::GetHostOrSpecFromURL(url));

  {
    base::AutoLock auto_lock(lock_);
    if (level == default_zoom_level_)
      host_zoom_levels_.erase(host);
    else
      host_zoom_levels_[host] = level;
  }

  NotificationService::current()->Notify(NotificationType::ZOOM_LEVEL_CHANGED,
                                         Source<Profile>(profile_),
                                         NotificationService::NoDetails());

  // If we're in incognito mode, don't persist changes to the prefs.  We'll keep
  // them in memory only so they will be forgotten on exiting incognito.
  if (profile_->IsOffTheRecord())
    return;

  updating_preferences_ = true;
  {
    ScopedPrefUpdate update(profile_->GetPrefs(), prefs::kPerHostZoomLevels);
    DictionaryValue* host_zoom_dictionary =
        profile_->GetPrefs()->GetMutableDictionary(prefs::kPerHostZoomLevels);
    if (level == default_zoom_level_) {
      host_zoom_dictionary->RemoveWithoutPathExpansion(host, NULL);
    } else {
      host_zoom_dictionary->SetWithoutPathExpansion(
          host, Value::CreateDoubleValue(level));
    }
  }
  updating_preferences_ = false;
}

double HostZoomMap::GetTemporaryZoomLevel(int render_process_id,
                                          int render_view_id) const {
  base::AutoLock auto_lock(lock_);
  for (size_t i = 0; i < temporary_zoom_levels_.size(); ++i) {
    if (temporary_zoom_levels_[i].render_process_id == render_process_id &&
        temporary_zoom_levels_[i].render_view_id == render_view_id) {
      return temporary_zoom_levels_[i].zoom_level;
    }
  }
  return 0;
}

void HostZoomMap::SetTemporaryZoomLevel(int render_process_id,
                                        int render_view_id,
                                        double level) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!profile_)
    return;

  {
    base::AutoLock auto_lock(lock_);
    size_t i;
    for (i = 0; i < temporary_zoom_levels_.size(); ++i) {
      if (temporary_zoom_levels_[i].render_process_id == render_process_id &&
          temporary_zoom_levels_[i].render_view_id == render_view_id) {
        if (level) {
          temporary_zoom_levels_[i].zoom_level = level;
        } else {
          temporary_zoom_levels_.erase(temporary_zoom_levels_.begin() + i);
        }
        break;
      }
    }

    if (level && i == temporary_zoom_levels_.size()) {
      TemporaryZoomLevel temp;
      temp.render_process_id = render_process_id;
      temp.render_view_id = render_view_id;
      temp.zoom_level = level;
      temporary_zoom_levels_.push_back(temp);
    }
  }

  NotificationService::current()->Notify(NotificationType::ZOOM_LEVEL_CHANGED,
                                         Source<Profile>(profile_),
                                         NotificationService::NoDetails());
}

void HostZoomMap::ResetToDefaults() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!profile_)
    return;

  {
    base::AutoLock auto_lock(lock_);
    host_zoom_levels_.clear();
  }

  updating_preferences_ = true;
  profile_->GetPrefs()->ClearPref(prefs::kPerHostZoomLevels);
  updating_preferences_ = false;
}

void HostZoomMap::Shutdown() {
  if (!profile_)
    return;

  registrar_.RemoveAll();
  if (!profile_->IsOffTheRecord())
    pref_change_registrar_.RemoveAll();
  profile_ = NULL;
}

void HostZoomMap::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (type.value) {
    case NotificationType::PROFILE_DESTROYED:
      // If the profile is going away, we need to stop using it.
      Shutdown();
      break;
    case NotificationType::RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW: {
      base::AutoLock auto_lock(lock_);
      int render_view_id = Source<RenderViewHost>(source)->routing_id();
      int render_process_id = Source<RenderViewHost>(source)->process()->id();

      for (size_t i = 0; i < temporary_zoom_levels_.size(); ++i) {
        if (temporary_zoom_levels_[i].render_process_id == render_process_id &&
            temporary_zoom_levels_[i].render_view_id == render_view_id) {
          temporary_zoom_levels_.erase(temporary_zoom_levels_.begin() + i);
          break;
        }
      }
      break;
    }
    case NotificationType::PREF_CHANGED: {
      // If we are updating our own preference, don't reload.
      if (!updating_preferences_) {
        std::string* name = Details<std::string>(details).ptr();
        if (prefs::kPerHostZoomLevels == *name)
          Load();
        else if (prefs::kDefaultZoomLevel == *name) {
          default_zoom_level_ =
              profile_->GetPrefs()->GetDouble(prefs::kDefaultZoomLevel);
        }
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected preference observed.";
  }
}

HostZoomMap::~HostZoomMap() {
  Shutdown();
}
