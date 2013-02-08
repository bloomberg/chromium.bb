// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/shell_window_geometry_cache.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace {

// The timeout in milliseconds before we'll persist window geometry to the
// StateStore.
const int kSyncTimeoutMilliseconds = 1000;

} // namespace

namespace extensions {

ShellWindowGeometryCache::ShellWindowGeometryCache(Profile* profile,
                                                   ExtensionPrefs* prefs)
    : prefs_(prefs),
      sync_delay_(base::TimeDelta::FromMilliseconds(kSyncTimeoutMilliseconds)) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
}

ShellWindowGeometryCache::~ShellWindowGeometryCache() {
  SyncToStorage();
}

void ShellWindowGeometryCache::SaveGeometry(
    const std::string& extension_id,
    const std::string& window_id,
    const gfx::Rect& bounds) {
  ExtensionData& extension_data = cache_[extension_id];

  // If we don't have any unsynced changes and this is a duplicate of what's
  // already in the cache, just ignore it.
  if (extension_data[window_id].bounds == bounds &&
      !ContainsKey(unsynced_extensions_, extension_id))
    return;

  base::Time now = base::Time::Now();

  extension_data[window_id].bounds = bounds;
  extension_data[window_id].last_change = now;

  if (extension_data.size() > kMaxCachedWindows) {
    ExtensionData::iterator oldest = extension_data.end();
    // Too many windows in the cache, find the oldest one to remove.
    for (ExtensionData::iterator it = extension_data.begin();
         it != extension_data.end(); ++it) {
      // Don't expunge the window that was just added.
      if (it->first == window_id) continue;

      // If time is in the future, reset it to now to minimize weirdness.
      if (it->second.last_change > now)
        it->second.last_change = now;

      if (oldest == extension_data.end() ||
          it->second.last_change < oldest->second.last_change)
        oldest = it;
    }
    extension_data.erase(oldest);
  }

  unsynced_extensions_.insert(extension_id);

  // We don't use Reset() because the timer may not yet be running.
  // (In that case Stop() is a no-op.)
  sync_timer_.Stop();
  sync_timer_.Start(FROM_HERE, sync_delay_, this,
                    &ShellWindowGeometryCache::SyncToStorage);
}

void ShellWindowGeometryCache::SyncToStorage() {
  std::set<std::string> tosync;
  tosync.swap(unsynced_extensions_);
  for (std::set<std::string>::const_iterator it = tosync.begin(),
      eit = tosync.end(); it != eit; ++it) {
    const std::string& extension_id = *it;
    const ExtensionData& extension_data = cache_[extension_id];

    scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    for (ExtensionData::const_iterator it = extension_data.begin(),
         eit = extension_data.end(); it != eit; ++it) {
      base::DictionaryValue* value = new base::DictionaryValue;
      const gfx::Rect& bounds = it->second.bounds;
      value->SetInteger("x", bounds.x());
      value->SetInteger("y", bounds.y());
      value->SetInteger("w", bounds.width());
      value->SetInteger("h", bounds.height());
      value->SetString(
          "ts", base::Int64ToString(it->second.last_change.ToInternalValue()));
      dict->SetWithoutPathExpansion(it->first, value);
    }
    prefs_->SetGeometryCache(extension_id, dict.Pass());
  }
}

bool ShellWindowGeometryCache::GetGeometry(
    const std::string& extension_id,
    const std::string& window_id,
    gfx::Rect* bounds) const {

  std::map<std::string, ExtensionData>::const_iterator
      extension_data_it = cache_.find(extension_id);

  // Not in the map means loading data for the extension didn't finish yet.
  if (extension_data_it == cache_.end())
    return false;

  ExtensionData::const_iterator window_data = extension_data_it->second.find(
      window_id);

  if (window_data == extension_data_it->second.end())
    return false;

  *bounds = window_data->second.bounds;
  return true;
}

void ShellWindowGeometryCache::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      std::string extension_id =
          content::Details<const Extension>(details).ptr()->id();
      OnExtensionLoaded(extension_id);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      std::string extension_id =
          content::Details<const UnloadedExtensionInfo>(details).
              ptr()->extension->id();
      OnExtensionUnloaded(extension_id);
      break;
    }
    default:
      NOTREACHED();
      return;
  }
}

void ShellWindowGeometryCache::SetSyncDelayForTests(int timeout_ms) {
  sync_delay_ = base::TimeDelta::FromMilliseconds(timeout_ms);
}

void ShellWindowGeometryCache::OnExtensionLoaded(
    const std::string& extension_id) {
  ExtensionData& extension_data = cache_[extension_id];

  const base::DictionaryValue* stored_windows =
      prefs_->GetGeometryCache(extension_id);
  if (!stored_windows)
    return;

  for (base::DictionaryValue::Iterator it(*stored_windows); it.HasNext();
       it.Advance()) {
    // If the cache already contains geometry for this window, don't
    // overwrite that information since it is probably the result of an
    // application starting up very quickly.
    const std::string& window_id = it.key();
    ExtensionData::iterator cached_window =
        extension_data.find(window_id);
    if (cached_window == extension_data.end()) {
      const base::DictionaryValue* stored_window;
      if (it.value().GetAsDictionary(&stored_window)) {
        WindowData& window_data = extension_data[it.key()];

        int i;
        if (stored_window->GetInteger("x", &i))
          window_data.bounds.set_x(i);
        if (stored_window->GetInteger("y", &i))
          window_data.bounds.set_y(i);
        if (stored_window->GetInteger("w", &i))
          window_data.bounds.set_width(i);
        if (stored_window->GetInteger("h", &i))
          window_data.bounds.set_height(i);
        std::string ts_as_string;
        if (stored_window->GetString("ts", &ts_as_string)) {
          int64 ts;
          if (base::StringToInt64(ts_as_string, &ts)) {
            window_data.last_change = base::Time::FromInternalValue(ts);
          }
        }
      }
    }
  }
}

void ShellWindowGeometryCache::OnExtensionUnloaded(
    const std::string& extension_id) {
  SyncToStorage();
  cache_.erase(extension_id);
}

} // namespace extensions
