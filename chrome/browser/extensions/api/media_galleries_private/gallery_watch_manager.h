// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Manages all the gallery file watchers for the associated profile. This class
// lives on the file thread. This class is instantiated per profile. This
// is temporary and will be moved to a permanent, public place in the near
// future. Please refer to crbug.com/166950 for more details.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_GALLERY_WATCH_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_GALLERY_WATCH_MANAGER_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"

namespace extensions {

class MediaGalleriesPrivateEventRouter;

class GalleryWatchManager {
 public:
  // Returns the GalleryWatchManager for |profile_id|, creating it if it is not
  // yet created.
  static GalleryWatchManager* GetForProfile(void* profile_id);

  // Returns true if an GalleryWatchManager already exists for the specified
  // |profile_id|.
  static bool HasForProfile(void* profile_id);

  // Notifies about the profile shutdown event.
  static void OnProfileShutdown(void* profile_id);

  // Sets up a gallery watch for the extension specified by the |extension_id|.
  // |profile_id| specifies the extension profile identifier.
  // |gallery_id| specifies the gallery identifier.
  // |watch_path| specifies the absolute gallery path.
  // Returns true, if the watch setup operation was successful.
  static bool SetupGalleryWatch(
      void* profile_id,
      chrome::MediaGalleryPrefId gallery_id,
      const base::FilePath& watch_path,
      const std::string& extension_id,
      base::WeakPtr<MediaGalleriesPrivateEventRouter> event_router);

  // Cancels the gallery watch for the extension specified by the
  // |extension_id|. |profile_id| specifies the extension profile identifier.
  // |watch_path| specifies the absolute gallery path.
  static void RemoveGalleryWatch(void* profile_id,
                                 const base::FilePath& watch_path,
                                 const std::string& extension_id);

  // Notifies about the extension unloaded/uninstalled event.
  static void OnExtensionUnloaded(void* profile_id,
                                  const std::string& extension_id);

 private:
  class GalleryFilePathWatcher;
  typedef std::map<base::FilePath, GalleryFilePathWatcher*> WatcherMap;

  // Use GetForProfile().
  GalleryWatchManager();
  ~GalleryWatchManager();

  // Initiates a gallery watch operation for the extension specified by
  // the |extension_id|. |gallery_id| specifies the gallery identifier and
  // |watch_path| specifies the absolute path of the gallery. Returns true,
  // if the watch was set successfully.
  bool StartGalleryWatch(
      chrome::MediaGalleryPrefId gallery_id,
      const base::FilePath& watch_path,
      const std::string& extension_id,
      base::WeakPtr<MediaGalleriesPrivateEventRouter> event_router);

  // Cancels the gallery watch operation for the extension specified by the
  // |extension_id|. |watch_path| specifies the absolute path of the gallery.
  void StopGalleryWatch(const base::FilePath& watch_path,
                        const std::string& extension_id);

  // Handles the extension unloaded/uninstalled event.
  void HandleExtensionUnloadedEvent(const std::string& extension_id);

  // Deletes the gallery watchers.
  void DeleteAllWatchers();

  // Removes the GalleryFilePathWatcher entry associated with the given
  // |watch_path|.
  void RemoveGalleryFilePathWatcherEntry(const base::FilePath& watch_path);

  // Map to manage the gallery file path watchers.
  // Key: Gallery watch path.
  // Value: GalleryFilePathWatcher*.
  WatcherMap gallery_watchers_;

  DISALLOW_COPY_AND_ASSIGN(GalleryWatchManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_PRIVATE_GALLERY_WATCH_MANAGER_H_
