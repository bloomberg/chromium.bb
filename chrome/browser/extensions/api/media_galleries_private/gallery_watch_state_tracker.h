// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GalleryWatchStateTracker tracks the gallery watchers, updates the
// extension state storage and observes the notification events.
// GalleryWatchStateTracker lives on the UI thread.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_GALLERY_WATCH_STATE_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_GALLERY_WATCH_STATE_TRACKER_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace base {
class Value;
}

namespace extensions {

class GalleryWatchStateTracker
    : public content::NotificationObserver,
      public base::SupportsWeakPtr<GalleryWatchStateTracker> {
 public:
  explicit GalleryWatchStateTracker(Profile* profile);
  virtual ~GalleryWatchStateTracker();

  // Returns the GalleryWatchStateTracker associated with the |profile|.
  // Returns NULL if GalleryWatchStateTracker does not exists.
  static GalleryWatchStateTracker* GetForProfile(Profile* profile);

  // Updates the storage for the given extension on the receipt of gallery
  // watch added event.
  void OnGalleryWatchAdded(const std::string& extension_id,
                           chrome::MediaGalleryPrefId gallery_id);

  // Updates the storage for the given extension on the receipt of gallery
  // watch removed event.
  void OnGalleryWatchRemoved(const std::string& extension_id,
                             chrome::MediaGalleryPrefId gallery_id);

  // Updates the state of the gallery watchers on the receipt of access
  // permission changed event for the extension specified by the |extension_id|.
  // |has_permission| is set to true if the user granted permission to
  // access the gallery associated with the |gallery_id| and is set to false
  // if the user revoked the gallery permission.
  void OnGalleryPermissionChanged(const std::string& extension_id,
                                  chrome::MediaGalleryPrefId gallery_id,
                                  bool has_permission);

  // Returns a set of watched gallery identifiers for the extension specified
  // by the |extension_id|.
  chrome::MediaGalleryPrefIdSet GetAllWatchedGalleryIDsForExtension(
      const std::string& extension_id) const;

  // Removes all the gallery watchers associated with the extension specified
  // by the |extension_id|.
  void RemoveAllGalleryWatchersForExtension(const std::string& extension_id);

 private:
  // Key: Gallery identifier.
  // Value: True if the watcher is active. Watcher will be active only if
  // the extension has access permission to the watched gallery and a watcher
  // has been successfully setup.
  typedef std::map<chrome::MediaGalleryPrefId, bool> WatchedGalleriesMap;

  // Key: Extension identifier.
  // Value: Map of watched gallery information.
  typedef std::map<std::string, WatchedGalleriesMap> WatchedExtensionsMap;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Syncs media gallery watch data for the given extension to/from the state
  // storage.
  void WriteToStorage(const std::string& extension_id);
  void ReadFromStorage(const std::string& extension_id,
                       scoped_ptr<base::Value> value);

  // Sets up the gallery watcher on the receipt of granted gallery permission
  // event.
  void SetupGalleryWatch(const std::string& extension_id,
                         chrome::MediaGalleryPrefId gallery_id);

  // Removes the gallery watcher on the receipt of revoked gallery permission
  // event.
  void RemoveGalleryWatch(const std::string& extension_id,
                          chrome::MediaGalleryPrefId gallery_id);

  // Returns true if a gallery watcher exists for the extension.
  // Set |has_active_watcher| to true to find if the gallery watcher is active.
  bool HasGalleryWatchInfo(const std::string& extension_id,
                           chrome::MediaGalleryPrefId gallery_id,
                           bool has_active_watcher);

  // Handles the setup gallery watch request response. When an extension is
  // loaded, GalleryWatchStateTracker reads the storage and sends request to
  // setup gallery watchers for the known watch paths.
  void HandleSetupGalleryWatchResponse(const std::string& extension_id,
                                       chrome::MediaGalleryPrefId gallery_id,
                                       bool success);

  // Adds the watched |gallery_id| in |watched_extensions_map_| for the
  // extension specified by the |extension_id|. Returns true if the |gallery_id|
  // is added into the |watched_extensions_map_| and returns false if the
  // |gallery_id| already exists in the |watched_extensions_map_|.
  bool AddWatchedGalleryIdInfoForExtension(
      const std::string& extension_id,
      chrome::MediaGalleryPrefId gallery_id);

  // Current profile.
  Profile* profile_;

  // Used to listen for NOTIFICATION_EXTENSION_LOADED and
  // NOTIFICATION_EXTENSION_UNLOADED events.
  content::NotificationRegistrar registrar_;

  // A map of watched gallery details, per extension.
  WatchedExtensionsMap watched_extensions_map_;

  DISALLOW_COPY_AND_ASSIGN(GalleryWatchStateTracker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_GALLERY_WATCH_STATE_TRACKER_H_
