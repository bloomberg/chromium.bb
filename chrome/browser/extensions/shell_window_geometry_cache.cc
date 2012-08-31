// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/shell_window_geometry_cache.h"

#include "base/bind.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace {

// Keys for serialization to and from Value to store in the preferences.
const char kWindowGeometryKey[] = "window_geometry";

// The timeout in milliseconds before we'll persist window geometry to the
// StateStore.
const int kSyncTimeoutMilliseconds = 1000;

} // namespace

namespace extensions {

ShellWindowGeometryCache::ShellWindowGeometryCache(Profile* profile,
                                                   StateStore* state_store)
    : store_(state_store) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));

  store_->RegisterKey(kWindowGeometryKey);
}

ShellWindowGeometryCache::~ShellWindowGeometryCache() {
  SyncToStorage();
}

void ShellWindowGeometryCache::SaveGeometry(
    const std::string& extension_id,
    const std::string& window_id,
    const gfx::Rect& bounds) {
  std::map<std::string, gfx::Rect>& geometry = cache_[extension_id];
  geometry[window_id] = bounds;

  unsynced_extensions_.insert(extension_id);

  // We don't use Reset() because the timer may not yet be running.
  // (In that case Stop() is a no-op.)
  sync_timer_.Stop();
  sync_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kSyncTimeoutMilliseconds), this,
      &ShellWindowGeometryCache::SyncToStorage);
}

void ShellWindowGeometryCache::SyncToStorage() {
  std::set<std::string> tosync;
  tosync.swap(unsynced_extensions_);
  for (std::set<std::string>::const_iterator it = tosync.begin(),
      eit = tosync.end(); it != eit; ++it) {
    const std::string& extension_id = *it;
    const std::map<std::string, gfx::Rect>& geometry = cache_[extension_id];

    scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    for (std::map<std::string, gfx::Rect>::const_iterator it =
        geometry.begin(), eit = geometry.end(); it != eit; ++it) {
      base::DictionaryValue* value = new base::DictionaryValue;
      const gfx::Rect& bounds = it->second;
      value->SetInteger("x", bounds.x());
      value->SetInteger("y", bounds.y());
      value->SetInteger("w", bounds.width());
      value->SetInteger("h", bounds.height());
      dict->SetWithoutPathExpansion(it->first, value);
    }
    store_->SetExtensionValue(extension_id, kWindowGeometryKey,
                              scoped_ptr<base::Value>(dict.release()));
  }
}

bool ShellWindowGeometryCache::GetGeometry(
    const std::string& extension_id,
    const std::string& window_id,
    gfx::Rect* bounds) const {

  std::map<std::string, std::map<std::string, gfx::Rect> >::const_iterator
      ext = cache_.find(extension_id);

  // Not in the map means loading data for the extension didn't finish yet.
  if (ext == cache_.end())
    return false;

  std::map<std::string, gfx::Rect>::const_iterator win = ext->second.find(
      window_id);

  if (win == ext->second.end())
    return false;

  *bounds = win->second;
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

void ShellWindowGeometryCache::OnExtensionLoaded(
    const std::string& extension_id)
{
  store_->GetExtensionValue(extension_id, kWindowGeometryKey,
          base::Bind(&ShellWindowGeometryCache::GeometryFromStorage,
          AsWeakPtr(), extension_id));
}

void ShellWindowGeometryCache::OnExtensionUnloaded(
    const std::string& extension_id)
{
  SyncToStorage();
  cache_.erase(extension_id);
}

void ShellWindowGeometryCache::GeometryFromStorage(
    const std::string& extension_id, scoped_ptr<base::Value> value) {
  // TODO(mek): What should happen if an extension is already unloaded by the
  // time this code is executed.
  std::map<std::string, gfx::Rect>& geometry = cache_[extension_id];

  base::DictionaryValue* dict;
  if (value.get() && value->GetAsDictionary(&dict)) {
    for (base::DictionaryValue::Iterator it(*dict); it.HasNext();
        it.Advance()) {
      // If the cache already contains geometry for this window, don't
      // overwrite that information since it is probably the result of an
      // application starting up very quickly.
      std::map<std::string, gfx::Rect>::iterator geomIt =
          geometry.find(it.key());
      if (geomIt == geometry.end()) {
        const base::DictionaryValue* geom;
        if (it.value().GetAsDictionary(&geom)) {
          gfx::Rect& bounds = geometry[it.key()];

          int i;
          if (geom->GetInteger("x", &i)) bounds.set_x(i);
          if (geom->GetInteger("y", &i)) bounds.set_y(i);
          if (geom->GetInteger("w", &i)) bounds.set_width(i);
          if (geom->GetInteger("h", &i)) bounds.set_height(i);
        }
      }
    }
  }
}


} // namespace extensions
