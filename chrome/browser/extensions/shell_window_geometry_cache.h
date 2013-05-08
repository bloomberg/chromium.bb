// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SHELL_WINDOW_GEOMETRY_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_SHELL_WINDOW_GEOMETRY_CACHE_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/values.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/rect.h"

class Profile;

namespace extensions {

class ExtensionPrefs;

// A cache for persisted geometry of shell windows, both to not have to wait
// for IO when creating a new window, and to not cause IO on every window
// geometry change.
class ShellWindowGeometryCache
    : public base::SupportsWeakPtr<ShellWindowGeometryCache>,
      public content::NotificationObserver {
 public:
  ShellWindowGeometryCache(Profile* profile,
                           ExtensionPrefs* prefs);

  virtual ~ShellWindowGeometryCache();

  void SaveGeometry(const std::string& extension_id,
                    const std::string& window_id,
                    const gfx::Rect& bounds);

  bool GetGeometry(const std::string& extension_id,
                   const std::string& window_id,
                   gfx::Rect* bounds) const;

  // Maximum number of windows we'll cache the geometry for per app.
  static const size_t kMaxCachedWindows = 100;

 protected:
  friend class ShellWindowGeometryCacheTest;

  // For tests, this modifies the timeout delay for saving changes from calls
  // to SaveGeometry. (Note that even if this is set to 0, you still need to
  // run the message loop to see the results of any SyncToStorage call).
  void SetSyncDelayForTests(int timeout_ms);

 private:
  // Data stored for each window.
  struct WindowData {
    gfx::Rect bounds;
    base::Time last_change;
  };

  // Data stored for each extension.
  typedef std::map<std::string, WindowData> ExtensionData;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void OnExtensionLoaded(const std::string& extension_id);
  void OnExtensionUnloaded(const std::string& extension_id);
  void SyncToStorage();

  // Preferences storage.
  ExtensionPrefs* prefs_;

  // Cached data
  std::map<std::string, ExtensionData> cache_;

  // Data that still needs saving
  std::set<std::string> unsynced_extensions_;

  // The timer used to save the data
  base::OneShotTimer<ShellWindowGeometryCache> sync_timer_;

  // The timeout value we'll use for |sync_timer_|.
  base::TimeDelta sync_delay_;

  content::NotificationRegistrar registrar_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SHELL_WINDOW_GEOMETRY_CACHE_H_
