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
#include "base/system_monitor/system_monitor.h"
#include "chrome/browser/media_gallery/media_galleries_preferences.h"
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

  // Updates the checkbox state for |gallery|. |gallery| is owned by the
  // controller and is guaranteed to live longer than the dialog. If the
  // checkbox doesn't already exist, it should be created.
  virtual void UpdateGallery(const MediaGalleryPrefInfo* gallery,
                             bool permitted) = 0;

  // Constructs a platform-specific dialog owned and controlled by |controller|.
  static MediaGalleriesDialog* Create(
      MediaGalleriesDialogController* controller);
};

// The controller is responsible for handling the logic of the dialog and
// interfacing with the model (i.e., MediaGalleriesPreferences). It shows
// the dialog and owns itself.
class MediaGalleriesDialogController
    : public ui::SelectFileDialog::Listener,
      public base::SystemMonitor::DevicesChangedObserver {
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
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;

  // base::SystemMonitor::DevicesChangedObserver implementation:
  virtual void OnRemovableStorageAttached(
      const std::string& id,
      const string16& name,
      const FilePath::StringType& location) OVERRIDE;
  virtual void OnRemovableStorageDetached(const std::string& id) OVERRIDE;

  // Populates |known_galleries_|.
  void InitializePermissions();

  // Saves state of |known_galleries_| and |new_galleries_| to model.
  void SavePermissions();

  // Update the model and view when a device is attached or detached.
  void UpdateGalleryOnDeviceEvent(const std::string& device_id, bool attached);

  // The web contents from which the request originated.
  content::WebContents* web_contents_;

  // This is just a reference, but it's assumed that it won't become invalid
  // while the dialog is showing. Will be NULL only during tests.
  const extensions::Extension* extension_;

  // This map excludes those galleries which have been blacklisted; it only
  // counts active known galleries.
  KnownGalleryPermissions known_galleries_;
  NewGalleryPermissions new_galleries_;

  // We run this callback when done.
  base::Closure on_finish_;

  // The model that tracks galleries and extensions' permissions.
  MediaGalleriesPreferences* preferences_;

  // The view that's showing.
  scoped_ptr<MediaGalleriesDialog> dialog_;

  scoped_refptr<ui::SelectFileDialog> select_folder_dialog_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesDialogController);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_GALLERIES_DIALOG_CONTROLLER_H_
