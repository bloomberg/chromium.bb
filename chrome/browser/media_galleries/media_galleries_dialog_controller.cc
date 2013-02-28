// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_dialog_controller.h"

#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::Extension;

namespace chrome {

namespace {

bool IsAttachedDevice(const std::string& device_id) {
  if (!MediaStorageUtil::IsRemovableDevice(device_id))
    return false;

  std::vector<StorageMonitor::StorageInfo> removable_storages =
      StorageMonitor::GetInstance()->GetAttachedStorage();
  for (size_t i = 0; i < removable_storages.size(); ++i) {
    if (removable_storages[i].device_id == device_id)
      return true;
  }
  return false;
}

}  // namespace

MediaGalleriesDialogController::MediaGalleriesDialogController(
    content::WebContents* web_contents,
    const Extension& extension,
    const base::Closure& on_finish)
      : web_contents_(web_contents),
        extension_(&extension),
        on_finish_(on_finish) {
  MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();
  preferences_ = registry->GetPreferences(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  InitializePermissions();

  dialog_.reset(MediaGalleriesDialog::Create(this));

  StorageMonitor* monitor = StorageMonitor::GetInstance();
  if (monitor)
    monitor->AddObserver(this);

  preferences_->AddGalleryChangeObserver(this);
}

MediaGalleriesDialogController::MediaGalleriesDialogController()
    : web_contents_(NULL),
      extension_(NULL),
      preferences_(NULL) {}

MediaGalleriesDialogController::~MediaGalleriesDialogController() {
  StorageMonitor* monitor = StorageMonitor::GetInstance();
  if (monitor)
    monitor->RemoveObserver(this);

  if (select_folder_dialog_.get())
    select_folder_dialog_->ListenerDestroyed();
}

// static
string16 MediaGalleriesDialogController::GetGalleryDisplayName(
    const MediaGalleryPrefInfo& gallery) {
  string16 name = gallery.display_name;
  if (IsAttachedDevice(gallery.device_id)) {
    name += ASCIIToUTF16(" ");
    name +=
        l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_DIALOG_DEVICE_ATTACHED);
  }
  return name;
}

// static
string16 MediaGalleriesDialogController::GetGalleryTooltip(
    const MediaGalleryPrefInfo& gallery) {
  return gallery.AbsolutePath().LossyDisplayName();
}

string16 MediaGalleriesDialogController::GetHeader() const {
  std::string extension_name(extension_ ? extension_->name() : "");
  return l10n_util::GetStringFUTF16(IDS_MEDIA_GALLERIES_DIALOG_HEADER,
                                    UTF8ToUTF16(extension_name));
}

string16 MediaGalleriesDialogController::GetSubtext() const {
  std::string extension_name(extension_ ? extension_->name() : "");
  return l10n_util::GetStringFUTF16(IDS_MEDIA_GALLERIES_DIALOG_SUBTEXT,
                                    UTF8ToUTF16(extension_name));
}

bool MediaGalleriesDialogController::HasPermittedGalleries() const {
  for (KnownGalleryPermissions::const_iterator iter = permissions().begin();
       iter != permissions().end(); ++iter) {
    if (iter->second.allowed)
      return true;
  }
  return false;
}

void MediaGalleriesDialogController::OnAddFolderClicked() {
  base::FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  select_folder_dialog_ =
      ui::SelectFileDialog::Create(this, new ChromeSelectFilePolicy(NULL));
  select_folder_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_DIALOG_ADD_GALLERY_TITLE),
      user_data_dir,
      NULL, 0, FILE_PATH_LITERAL(""),
      web_contents_->GetView()->GetTopLevelNativeWindow(),
      NULL);
}

void MediaGalleriesDialogController::DidToggleGallery(
    const MediaGalleryPrefInfo* gallery,
    bool enabled) {
  // Check known galleries.
  KnownGalleryPermissions::iterator iter =
      known_galleries_.find(gallery->pref_id);
  if (iter != known_galleries_.end()) {
    DCHECK_NE(iter->second.allowed, enabled);
    iter->second.allowed = enabled;
    if (ContainsKey(toggled_galleries_, gallery->pref_id))
      toggled_galleries_.erase(gallery->pref_id);
    else
      toggled_galleries_.insert(gallery->pref_id);
    return;
  }

  // Check new galleries.
  for (NewGalleryPermissions::iterator iter = new_galleries_.begin();
       iter != new_galleries_.end(); ++iter) {
    if (&iter->pref_info == gallery) {
      iter->allowed = enabled;
      return;
    }
  }

  NOTREACHED();
}

void MediaGalleriesDialogController::DialogFinished(bool accepted) {
  // The dialog has finished, so there is no need to watch for more updates
  // from |preferences_|. Do this here and not in the dtor since this is the
  // only non-test code path that deletes |this|. The test ctor never adds
  // this observer in the first place.
  preferences_->RemoveGalleryChangeObserver(this);

  if (accepted)
    SavePermissions();

  on_finish_.Run();
  delete this;
}

const MediaGalleriesDialogController::KnownGalleryPermissions&
MediaGalleriesDialogController::permissions() const {
  return known_galleries_;
}

content::WebContents* MediaGalleriesDialogController::web_contents() {
  return web_contents_;
}

void MediaGalleriesDialogController::FileSelected(const base::FilePath& path,
                                                  int /*index*/,
                                                  void* /*params*/) {
  // Try to find it in the prefs.
  MediaGalleryPrefInfo gallery;
  bool gallery_exists = preferences_->LookUpGalleryByPath(path, &gallery);
  if (gallery_exists && gallery.type != MediaGalleryPrefInfo::kBlackListed) {
    // The prefs are in sync with |known_galleries_|, so it should exist in
    // |known_galleries_| as well. User selecting a known gallery effectively
    // just sets the gallery to permitted.
    KnownGalleryPermissions::const_iterator iter =
        known_galleries_.find(gallery.pref_id);
    DCHECK(iter != known_galleries_.end());
    dialog_->UpdateGallery(&iter->second.pref_info, true);
    return;
  }

  // Try to find it in |new_galleries_| (user added same folder twice).
  for (NewGalleryPermissions::iterator iter = new_galleries_.begin();
       iter != new_galleries_.end(); ++iter) {
    if (iter->pref_info.path == gallery.path &&
        iter->pref_info.device_id == gallery.device_id) {
      iter->allowed = true;
      dialog_->UpdateGallery(&iter->pref_info, true);
      return;
    }
  }

  // Lastly, add a new gallery to |new_galleries_|.
  new_galleries_.push_back(GalleryPermission(gallery, true));
  dialog_->UpdateGallery(&new_galleries_.back().pref_info, true);
}

void MediaGalleriesDialogController::OnRemovableStorageAttached(
    const StorageMonitor::StorageInfo& info) {
  UpdateGalleriesOnDeviceEvent(info.device_id);
}

void MediaGalleriesDialogController::OnRemovableStorageDetached(
    const StorageMonitor::StorageInfo& info) {
  UpdateGalleriesOnDeviceEvent(info.device_id);
}

void MediaGalleriesDialogController::OnGalleryChanged(
    MediaGalleriesPreferences* pref, const std::string& extension_id) {
  DCHECK_EQ(preferences_, pref);
  if (extension_id.empty() || extension_id == extension_->id())
    UpdateGalleriesOnPreferencesEvent();
}

void MediaGalleriesDialogController::InitializePermissions() {
  const MediaGalleriesPrefInfoMap& galleries = preferences_->known_galleries();
  for (MediaGalleriesPrefInfoMap::const_iterator iter = galleries.begin();
       iter != galleries.end();
       ++iter) {
    const MediaGalleryPrefInfo& gallery = iter->second;
    if (gallery.type == MediaGalleryPrefInfo::kBlackListed)
      continue;

    known_galleries_[iter->first] = GalleryPermission(gallery, false);
  }

  MediaGalleryPrefIdSet permitted =
      preferences_->GalleriesForExtension(*extension_);

  for (MediaGalleryPrefIdSet::iterator iter = permitted.begin();
       iter != permitted.end(); ++iter) {
    if (ContainsKey(toggled_galleries_, *iter))
      continue;
    DCHECK(ContainsKey(known_galleries_, *iter));
    known_galleries_[*iter].allowed = true;
  }
}

void MediaGalleriesDialogController::SavePermissions() {
  for (KnownGalleryPermissions::const_iterator iter = known_galleries_.begin();
       iter != known_galleries_.end(); ++iter) {
    preferences_->SetGalleryPermissionForExtension(
        *extension_, iter->first, iter->second.allowed);
  }

  for (NewGalleryPermissions::const_iterator iter = new_galleries_.begin();
       iter != new_galleries_.end(); ++iter) {
    // If the user added a gallery then unchecked it, forget about it.
    if (!iter->allowed)
      continue;

    // TODO(gbillock): Should be adding volume metadata during FileSelected.
    const MediaGalleryPrefInfo& gallery = iter->pref_info;
    MediaGalleryPrefId id = preferences_->AddGalleryWithName(
        gallery.device_id, gallery.display_name, gallery.path, true);
    preferences_->SetGalleryPermissionForExtension(*extension_, id, true);
  }
}

void MediaGalleriesDialogController::UpdateGalleriesOnPreferencesEvent() {
  // Merge in the permissions from |preferences_|. Afterwards,
  // |known_galleries_| may contain galleries that no longer belong there,
  // but the code below will put |known_galleries_| back in a consistent state.
  InitializePermissions();

  // If a gallery no longer belongs in |known_galleries_|, forget it in the
  // model/view.
  // If a gallery still belong in |known_galleries_|, check for a duplicate
  // entry in |new_galleries_|, merge its permission and remove it. Then update
  // the view.
  const MediaGalleriesPrefInfoMap& pref_galleries =
      preferences_->known_galleries();
  MediaGalleryPrefIdSet galleries_to_forget;
  for (KnownGalleryPermissions::iterator it = known_galleries_.begin();
       it != known_galleries_.end();
       ++it) {
    const MediaGalleryPrefId& gallery_id = it->first;
    GalleryPermission& gallery = it->second;
    MediaGalleriesPrefInfoMap::const_iterator pref_it =
        pref_galleries.find(gallery_id);
    // Check for lingering entry that should be removed.
    if (pref_it == pref_galleries.end() ||
        pref_it->second.type == MediaGalleryPrefInfo::kBlackListed) {
      galleries_to_forget.insert(gallery_id);
      dialog_->ForgetGallery(&gallery.pref_info);
      continue;
    }

    // Look for duplicate entries in |new_galleries_|.
    for (NewGalleryPermissions::iterator new_it = new_galleries_.begin();
         new_it != new_galleries_.end();
         ++new_it) {
      if (new_it->pref_info.path == gallery.pref_info.path &&
          new_it->pref_info.device_id == gallery.pref_info.device_id) {
        // Found duplicate entry. Get the existing permission from it and then
        // remove it.
        gallery.allowed = new_it->allowed;
        dialog_->ForgetGallery(&new_it->pref_info);
        new_galleries_.erase(new_it);
        break;
      }
    }
    dialog_->UpdateGallery(&gallery.pref_info, gallery.allowed);
  }

  // Remove the galleries to forget from |known_galleries_|. Doing it in the
  // above loop would invalidate the iterator there.
  for (MediaGalleryPrefIdSet::const_iterator it = galleries_to_forget.begin();
       it != galleries_to_forget.end();
       ++it) {
    known_galleries_.erase(*it);
  }
}

void MediaGalleriesDialogController::UpdateGalleriesOnDeviceEvent(
    const std::string& device_id) {
  for (KnownGalleryPermissions::iterator iter = known_galleries_.begin();
       iter != known_galleries_.end(); ++iter) {
    if (iter->second.pref_info.device_id == device_id)
      dialog_->UpdateGallery(&iter->second.pref_info, iter->second.allowed);
  }

  for (NewGalleryPermissions::iterator iter = new_galleries_.begin();
       iter != new_galleries_.end(); ++iter) {
    if (iter->pref_info.device_id == device_id)
      dialog_->UpdateGallery(&iter->pref_info, iter->allowed);
  }
}

// MediaGalleries dialog -------------------------------------------------------

MediaGalleriesDialog::~MediaGalleriesDialog() {}

}  // namespace chrome
