// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERIES_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERIES_DIALOG_CONTROLLER_H_

#include <list>
#include <map>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/media_gallery/media_galleries_preferences.h"
#include "chrome/browser/storage_monitor/removable_storage_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace chrome {

class MediaGalleriesDialogController;

// The view.
class MediaGalleriesDialog {
 public:
  virtual ~MediaGalleriesDialog();

  // Updates the entry for |gallery| with the checkbox set to the value in
  // |permitted|. |gallery| is owned by the controller and is guaranteed to
  // live longer than the dialog. If the entry does not already exist, it
  // should be created.
  virtual void UpdateGallery(const MediaGalleryPrefInfo* gallery,
                             bool permitted) = 0;

  // If there exists an entry for |gallery|, it should be removed.
  virtual void ForgetGallery(const MediaGalleryPrefInfo* gallery) = 0;

  // Constructs a platform-specific dialog owned and controlled by |controller|.
  static MediaGalleriesDialog* Create(
      MediaGalleriesDialogController* controller);
};

// The controller is responsible for handling the logic of the dialog and
// interfacing with the model (i.e., MediaGalleriesPreferences). It shows
// the dialog and owns itself.
class MediaGalleriesDialogController
    : public ui::SelectFileDialog::Listener,
      public RemovableStorageObserver,
      public MediaGalleriesPreferences::GalleryChangeObserver {
 public:
  // A fancy pair.
  struct GalleryPermission {
    GalleryPermission(const MediaGalleryPrefInfo& pref_info, bool allowed)
        : pref_info(pref_info), allowed(allowed) {}
    GalleryPermission() {}

    MediaGalleryPrefInfo pref_info;
    bool allowed;
  };

  // This type keeps track of media galleries already known to the prefs system.
  typedef std::map<MediaGalleryPrefId, GalleryPermission>
      KnownGalleryPermissions;

  // The constructor creates a dialog controller which owns itself.
  MediaGalleriesDialogController(content::WebContents* web_contents,
                                 const extensions::Extension& extension,
                                 const base::Closure& on_finish);

  // Called by the view.
  static string16 GetGalleryDisplayName(const MediaGalleryPrefInfo& gallery);
  static string16 GetGalleryTooltip(const MediaGalleryPrefInfo& gallery);
  virtual string16 GetHeader() const;
  virtual string16 GetSubtext() const;
  virtual bool HasPermittedGalleries() const;
  virtual void OnAddFolderClicked();
  virtual void DidToggleGallery(const MediaGalleryPrefInfo* pref_info,
                                bool enabled);
  virtual void DialogFinished(bool accepted);
  virtual const KnownGalleryPermissions& permissions() const;
  virtual content::WebContents* web_contents();

 protected:
  // For use with tests.
  MediaGalleriesDialogController();

  virtual ~MediaGalleriesDialogController();

 private:
  // This type is for media galleries that have been added via "add gallery"
  // button, but have not yet been committed to the prefs system and will be
  // forgotten if the user Cancels. Since they don't have IDs assigned yet, it's
  // just a list and not a map.
  typedef std::list<GalleryPermission> NewGalleryPermissions;

  // SelectFileDialog::Listener implementation:
  virtual void FileSelected(const base::FilePath& path,
                            int index,
                            void* params) OVERRIDE;

  // RemovableStorageObserver implementation.
  // Used to keep dialog in sync with removable device status.
  virtual void OnRemovableStorageAttached(
      const StorageMonitor::StorageInfo& info) OVERRIDE;
  virtual void OnRemovableStorageDetached(
      const StorageMonitor::StorageInfo& info) OVERRIDE;

  // MediaGalleriesPreferences::GalleryChangeObserver implementation.
  // Used to keep the dialog in sync when the preferences change.
  virtual void OnGalleryChanged(MediaGalleriesPreferences* pref,
                                const std::string& extension_id) OVERRIDE;

  // Populates |known_galleries_| from |preferences_|. Subsequent calls merge
  // into |known_galleries_| and do not change permissions for user toggled
  // galleries.
  void InitializePermissions();

  // Saves state of |known_galleries_| and |new_galleries_| to model.
  void SavePermissions();

  // Updates the model and view when |preferences_| changes. Some of the
  // possible changes includes a gallery getting blacklisted, or a new
  // auto detected gallery becoming available.
  void UpdateGalleriesOnPreferencesEvent();

  // Updates the model and view when a device is attached or detached.
  void UpdateGalleriesOnDeviceEvent(const std::string& device_id);

  // The web contents from which the request originated.
  content::WebContents* web_contents_;

  // This is just a reference, but it's assumed that it won't become invalid
  // while the dialog is showing. Will be NULL only during tests.
  const extensions::Extension* extension_;

  // This map excludes those galleries which have been blacklisted; it only
  // counts active known galleries.
  KnownGalleryPermissions known_galleries_;

  // Galleries in |known_galleries_| that the user have toggled.
  MediaGalleryPrefIdSet toggled_galleries_;

  // Map of new galleries the user added, but have not saved. This list should
  // never overlap with |known_galleries_|.
  NewGalleryPermissions new_galleries_;

  // Callback to run when the dialog closes.
  base::Closure on_finish_;

  // The model that tracks galleries and extensions' permissions.
  // This is the authoritative source for gallery information.
  MediaGalleriesPreferences* preferences_;

  // The view that's showing.
  scoped_ptr<MediaGalleriesDialog> dialog_;

  scoped_refptr<ui::SelectFileDialog> select_folder_dialog_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesDialogController);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERIES_DIALOG_CONTROLLER_H_
