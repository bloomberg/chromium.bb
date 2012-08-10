// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/media_galleries_dialog_controller.h"

#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/media_gallery/media_galleries_preferences_factory.h"
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

MediaGalleriesDialogController::MediaGalleriesDialogController(
    TabContents* tab_contents,
    const Extension& extension,
    const base::Callback<void(void)>& on_finish)
      : tab_contents_(tab_contents),
        extension_(extension),
        on_finish_(on_finish),
        preferences_(MediaGalleriesPreferencesFactory::GetForProfile(
          tab_contents_->profile())) {
  LookUpPermissions();

  dialog_.reset(MediaGalleriesDialog::Create(this));
}

MediaGalleriesDialogController::~MediaGalleriesDialogController() {
  if (select_folder_dialog_.get())
    select_folder_dialog_->ListenerDestroyed();
}

string16 MediaGalleriesDialogController::GetHeader() {
  return l10n_util::GetStringFUTF16(IDS_MEDIA_GALLERIES_DIALOG_HEADER,
                                    UTF8ToUTF16(extension_.name()));
}

string16 MediaGalleriesDialogController::GetSubtext() {
  if (extension_.HasAPIPermission(
          extensions::APIPermission::kMediaGalleriesRead)) {
    return l10n_util::GetStringFUTF16(IDS_MEDIA_GALLERIES_DIALOG_READ_SUBTEXT,
                                      UTF8ToUTF16(extension_.name()));
  }
  // TODO(estade): handle write et al.
  return string16();
}

void MediaGalleriesDialogController::OnAddFolderClicked() {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  // TODO(estade): if file dialogs are disabled we need to handle it somehow.
  select_folder_dialog_ =
      ui::SelectFileDialog::Create(this, new ChromeSelectFilePolicy(NULL));
  select_folder_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_DIALOG_ADD_GALLERY_TITLE),
      user_data_dir,
      NULL, 0, FILE_PATH_LITERAL(""),
      tab_contents_->web_contents()->GetView()->GetTopLevelNativeWindow(),
      NULL);
}

void MediaGalleriesDialogController::GalleryToggled(
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

void MediaGalleriesDialogController::FileSelected(const FilePath& path,
                                                  int index,
                                                  void* params) {
  // Try to find it in |known_galleries_|.
  MediaGalleryPrefInfo gallery;
  if (preferences_->LookUpGalleryByPath(path, &gallery) &&
      gallery.type != MediaGalleryPrefInfo::kBlackListed) {
    KnownGalleryPermissions::iterator iter =
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

void MediaGalleriesDialogController::LookUpPermissions() {
  for (MediaGalleriesPrefInfoMap::const_iterator iter =
           preferences_->known_galleries().begin();
       iter != preferences_->known_galleries().end();
       ++iter) {
    if (iter->second.type == MediaGalleryPrefInfo::kBlackListed)
      continue;

    known_galleries_[iter->first] = GalleryPermission(iter->second, false);
  }

  std::set<MediaGalleryPrefId> permitted =
      preferences_->GalleriesForExtension(extension_);

  for (std::set<MediaGalleryPrefId>::iterator iter = permitted.begin();
       iter != permitted.end(); ++iter) {
    known_galleries_[*iter].allowed = true;
  }
}

void MediaGalleriesDialogController::SavePermissions() {
  for (KnownGalleryPermissions::iterator iter = known_galleries_.begin();
       iter != known_galleries_.end(); ++iter) {
    preferences_->SetGalleryPermissionForExtension(
        extension_, iter->first, iter->second.allowed);
  }

  for (NewGalleryPermissions::iterator iter = new_galleries_.begin();
       iter != new_galleries_.end(); ++iter) {
    // If the user added a gallery then unchecked it, forget about it.
    if (!iter->allowed)
      continue;

    const MediaGalleryPrefInfo& gallery = iter->pref_info;
    MediaGalleryPrefId id = preferences_->AddGallery(
        gallery.device_id, gallery.display_name, gallery.path, true);
    preferences_->SetGalleryPermissionForExtension(
        extension_, id, true);
  }
}

// MediaGalleries dialog -------------------------------------------------------

MediaGalleriesDialog::~MediaGalleriesDialog() {}

}  // namespace chrome
