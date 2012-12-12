// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/media_galleries_dialog_controller.h"

#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media_gallery/media_file_system_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
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

  std::vector<base::SystemMonitor::RemovableStorageInfo> removable_storages =
      base::SystemMonitor::Get()->GetAttachedRemovableStorage();
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

  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->AddDevicesChangedObserver(this);
}

MediaGalleriesDialogController::MediaGalleriesDialogController()
    : web_contents_(NULL),
      extension_(NULL),
      preferences_(NULL) {}

MediaGalleriesDialogController::~MediaGalleriesDialogController() {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->RemoveDevicesChangedObserver(this);

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
  FilePath user_data_dir;
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
    iter->second.allowed = enabled;
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

void MediaGalleriesDialogController::FileSelected(const FilePath& path,
                                                  int /*index*/,
                                                  void* /*params*/) {
  // Try to find it in |known_galleries_|.
  MediaGalleryPrefInfo gallery;
  bool gallery_exists = preferences_->LookUpGalleryByPath(path, &gallery);
  if (gallery_exists && gallery.type != MediaGalleryPrefInfo::kBlackListed) {
    KnownGalleryPermissions::const_iterator iter =
        known_galleries_.find(gallery.pref_id);

    if (iter == known_galleries_.end()) {
      // This is rare, but could happen if a gallery was not "known"
      // when the controller first initialized, but has since been added.
      known_galleries_[gallery.pref_id] = GalleryPermission(gallery, true);
      iter = known_galleries_.find(gallery.pref_id);
    }

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

  // Lastly, add it to |new_galleries_|.
  new_galleries_.push_back(GalleryPermission(gallery, true));
  dialog_->UpdateGallery(&new_galleries_.back().pref_info, true);
}

void MediaGalleriesDialogController::OnRemovableStorageAttached(
    const std::string& id,
    const string16& /*name*/,
    const FilePath::StringType& /*location*/) {
  UpdateGalleryOnDeviceEvent(id, true /* attached */);
}

void MediaGalleriesDialogController::OnRemovableStorageDetached(
    const std::string& id) {
  UpdateGalleryOnDeviceEvent(id, false /* detached */);
}

void MediaGalleriesDialogController::InitializePermissions() {
  const MediaGalleriesPrefInfoMap& galleries = preferences_->known_galleries();
  const MediaGalleryPrefIdSet permitted_galleries =
      preferences_->GalleriesForExtension(*extension_);

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

    const MediaGalleryPrefInfo& gallery = iter->pref_info;
    MediaGalleryPrefId id = preferences_->AddGallery(
        gallery.device_id, gallery.display_name, gallery.path, true);
    preferences_->SetGalleryPermissionForExtension(
        *extension_, id, true);
  }
}

void MediaGalleriesDialogController::UpdateGalleryOnDeviceEvent(
    const std::string& device_id, bool attached) {
  GalleryPermission* gallery = NULL;
  for (KnownGalleryPermissions::iterator iter = known_galleries_.begin();
       iter != known_galleries_.end(); ++iter) {
    if (iter->second.pref_info.device_id == device_id) {
      gallery = &iter->second;
      break;
    }
  }

  if (!gallery) {
    for (NewGalleryPermissions::iterator iter = new_galleries_.begin();
         iter != new_galleries_.end(); ++iter) {
      if (iter->pref_info.device_id == device_id) {
        gallery = &(*iter);
        break;
      }
    }
  }

  if (gallery)
    dialog_->UpdateGallery(&gallery->pref_info, gallery->allowed);
}

// MediaGalleries dialog -------------------------------------------------------

MediaGalleriesDialog::~MediaGalleriesDialog() {}

}  // namespace chrome
