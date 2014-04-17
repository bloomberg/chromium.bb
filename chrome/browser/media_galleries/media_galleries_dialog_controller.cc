// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_dialog_controller.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_histograms.h"
#include "chrome/browser/media_galleries/media_gallery_context_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/media_galleries_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/text/bytes_formatting.h"

using extensions::APIPermission;
using extensions::Extension;
using storage_monitor::StorageInfo;
using storage_monitor::StorageMonitor;

namespace {

// Comparator for sorting GalleryPermissionsVector -- sorts
// allowed galleries low, and then sorts by absolute path.
bool GalleriesVectorComparator(
    const MediaGalleriesDialogController::GalleryPermission& a,
    const MediaGalleriesDialogController::GalleryPermission& b) {
  if (a.allowed && !b.allowed)
    return true;
  if (!a.allowed && b.allowed)
    return false;

  return a.pref_info.AbsolutePath() < b.pref_info.AbsolutePath();
}

}  // namespace

MediaGalleriesDialogController::MediaGalleriesDialogController(
    content::WebContents* web_contents,
    const Extension& extension,
    const base::Closure& on_finish)
      : web_contents_(web_contents),
        extension_(&extension),
        on_finish_(on_finish),
        preferences_(
            g_browser_process->media_file_system_registry()->GetPreferences(
                GetProfile())),
        create_dialog_callback_(base::Bind(&MediaGalleriesDialog::Create)) {
  // Passing unretained pointer is safe, since the dialog controller
  // is self-deleting, and so won't be deleted until it can be shown
  // and then closed.
  preferences_->EnsureInitialized(
      base::Bind(&MediaGalleriesDialogController::OnPreferencesInitialized,
                 base::Unretained(this)));

  // Unretained is safe because |this| owns |context_menu_|.
  context_menu_.reset(
      new MediaGalleryContextMenu(
          base::Bind(&MediaGalleriesDialogController::DidForgetGallery,
                     base::Unretained(this))));
}

void MediaGalleriesDialogController::OnPreferencesInitialized() {
  if (StorageMonitor::GetInstance())
    StorageMonitor::GetInstance()->AddObserver(this);

  // |preferences_| may be NULL in tests.
  if (preferences_) {
    preferences_->AddGalleryChangeObserver(this);
    InitializePermissions();
  }

  dialog_.reset(create_dialog_callback_.Run(this));
}

MediaGalleriesDialogController::MediaGalleriesDialogController(
    const extensions::Extension& extension,
    MediaGalleriesPreferences* preferences,
    const CreateDialogCallback& create_dialog_callback,
    const base::Closure& on_finish)
    : web_contents_(NULL),
      extension_(&extension),
      on_finish_(on_finish),
      preferences_(preferences),
      create_dialog_callback_(create_dialog_callback) {
  OnPreferencesInitialized();
}

MediaGalleriesDialogController::~MediaGalleriesDialogController() {
  if (StorageMonitor::GetInstance())
    StorageMonitor::GetInstance()->RemoveObserver(this);

  // |preferences_| may be NULL in tests.
  if (preferences_)
    preferences_->RemoveGalleryChangeObserver(this);

  if (select_folder_dialog_.get())
    select_folder_dialog_->ListenerDestroyed();
}

base::string16 MediaGalleriesDialogController::GetHeader() const {
  return l10n_util::GetStringFUTF16(IDS_MEDIA_GALLERIES_DIALOG_HEADER,
                                    base::UTF8ToUTF16(extension_->name()));
}

base::string16 MediaGalleriesDialogController::GetSubtext() const {
  extensions::MediaGalleriesPermission::CheckParam copy_to_param(
      extensions::MediaGalleriesPermission::kCopyToPermission);
  extensions::MediaGalleriesPermission::CheckParam delete_param(
      extensions::MediaGalleriesPermission::kDeletePermission);
  bool has_copy_to_permission =
      extensions::PermissionsData::CheckAPIPermissionWithParam(
          extension_, APIPermission::kMediaGalleries, &copy_to_param);
  bool has_delete_permission =
      extensions::PermissionsData::CheckAPIPermissionWithParam(
          extension_, APIPermission::kMediaGalleries, &delete_param);

  int id;
  if (has_copy_to_permission)
    id = IDS_MEDIA_GALLERIES_DIALOG_SUBTEXT_READ_WRITE;
  else if (has_delete_permission)
    id = IDS_MEDIA_GALLERIES_DIALOG_SUBTEXT_READ_DELETE;
  else
    id = IDS_MEDIA_GALLERIES_DIALOG_SUBTEXT_READ_ONLY;

  return l10n_util::GetStringFUTF16(id, base::UTF8ToUTF16(extension_->name()));
}

base::string16 MediaGalleriesDialogController::GetUnattachedLocationsHeader()
    const {
  return l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_UNATTACHED_LOCATIONS);
}

bool MediaGalleriesDialogController::IsAcceptAllowed() const {
  if (!toggled_galleries_.empty() || !forgotten_galleries_.empty())
    return true;

  for (GalleryPermissionsMap::const_iterator iter = new_galleries_.begin();
       iter != new_galleries_.end();
       ++iter) {
    if (iter->second.allowed)
      return true;
  }

  return false;
}

// Note: sorts by display criterion: GalleriesVectorComparator.
MediaGalleriesDialogController::GalleryPermissionsVector
MediaGalleriesDialogController::FillPermissions(bool attached) const {
  GalleryPermissionsVector result;
  for (GalleryPermissionsMap::const_iterator iter = known_galleries_.begin();
       iter != known_galleries_.end(); ++iter) {
    if (!ContainsKey(forgotten_galleries_, iter->first) &&
        attached == iter->second.pref_info.IsGalleryAvailable()) {
      result.push_back(iter->second);
    }
  }
  for (GalleryPermissionsMap::const_iterator iter = new_galleries_.begin();
       iter != new_galleries_.end(); ++iter) {
    if (attached == iter->second.pref_info.IsGalleryAvailable()) {
      result.push_back(iter->second);
    }
  }

  std::sort(result.begin(), result.end(), GalleriesVectorComparator);
  return result;
}

MediaGalleriesDialogController::GalleryPermissionsVector
MediaGalleriesDialogController::AttachedPermissions() const {
  return FillPermissions(true);
}

MediaGalleriesDialogController::GalleryPermissionsVector
MediaGalleriesDialogController::UnattachedPermissions() const {
  return FillPermissions(false);
}

void MediaGalleriesDialogController::OnAddFolderClicked() {
  base::FilePath default_path =
      extensions::file_system_api::GetLastChooseEntryDirectory(
          extensions::ExtensionPrefs::Get(GetProfile()), extension_->id());
  if (default_path.empty())
    PathService::Get(base::DIR_USER_DESKTOP, &default_path);
  select_folder_dialog_ =
      ui::SelectFileDialog::Create(this, new ChromeSelectFilePolicy(NULL));
  select_folder_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_DIALOG_ADD_GALLERY_TITLE),
      default_path,
      NULL,
      0,
      base::FilePath::StringType(),
      web_contents_->GetView()->GetTopLevelNativeWindow(),
      NULL);
}

void MediaGalleriesDialogController::DidToggleGallery(
    GalleryDialogId gallery_id, bool enabled) {
  // Check known galleries.
  GalleryPermissionsMap::iterator iter = known_galleries_.find(gallery_id);
  if (iter != known_galleries_.end()) {
    if (iter->second.allowed == enabled)
      return;

    iter->second.allowed = enabled;
    if (ContainsKey(toggled_galleries_, gallery_id))
      toggled_galleries_.erase(gallery_id);
    else
      toggled_galleries_.insert(gallery_id);
    return;
  }

  iter = new_galleries_.find(gallery_id);
  if (iter != new_galleries_.end())
    iter->second.allowed = enabled;

  // Don't sort -- the dialog is open, and we don't want to adjust any
  // positions for future updates to the dialog contents until they are
  // redrawn.
}

void MediaGalleriesDialogController::DidForgetGallery(
    GalleryDialogId gallery_id) {
  media_galleries::UsageCount(media_galleries::DIALOG_FORGET_GALLERY);
  if (!new_galleries_.erase(gallery_id)) {
    DCHECK(ContainsKey(known_galleries_, gallery_id));
    forgotten_galleries_.insert(gallery_id);
  }
  dialog_->UpdateGalleries();
}

void MediaGalleriesDialogController::DialogFinished(bool accepted) {
  // The dialog has finished, so there is no need to watch for more updates
  // from |preferences_|.
  // |preferences_| may be NULL in tests.
  if (preferences_)
    preferences_->RemoveGalleryChangeObserver(this);

  if (accepted)
    SavePermissions();

  on_finish_.Run();

  delete this;
}

content::WebContents* MediaGalleriesDialogController::web_contents() {
  return web_contents_;
}

void MediaGalleriesDialogController::FileSelected(const base::FilePath& path,
                                                  int /*index*/,
                                                  void* /*params*/) {
  extensions::file_system_api::SetLastChooseEntryDirectory(
      extensions::ExtensionPrefs::Get(GetProfile()),
      extension_->id(),
      path);

  // Try to find it in the prefs.
  MediaGalleryPrefInfo gallery;
  DCHECK(preferences_);
  bool gallery_exists = preferences_->LookUpGalleryByPath(path, &gallery);
  if (gallery_exists && !gallery.IsBlackListedType()) {
    // The prefs are in sync with |known_galleries_|, so it should exist in
    // |known_galleries_| as well. User selecting a known gallery effectively
    // just sets the gallery to permitted.
    GalleryDialogId gallery_id = GetDialogId(gallery.pref_id);
    GalleryPermissionsMap::iterator iter = known_galleries_.find(gallery_id);
    DCHECK(iter != known_galleries_.end());
    iter->second.allowed = true;
    forgotten_galleries_.erase(gallery_id);
    dialog_->UpdateGalleries();
    return;
  }

  // Try to find it in |new_galleries_| (user added same folder twice).
  for (GalleryPermissionsMap::iterator iter = new_galleries_.begin();
       iter != new_galleries_.end(); ++iter) {
    if (iter->second.pref_info.path == gallery.path &&
        iter->second.pref_info.device_id == gallery.device_id) {
      iter->second.allowed = true;
      dialog_->UpdateGalleries();
      return;
    }
  }

  // Lastly, if not found, add a new gallery to |new_galleries_|.
  // Note that it will have prefId = kInvalidMediaGalleryPrefId.
  GalleryDialogId gallery_id = GetDialogId(gallery.pref_id);
  new_galleries_[gallery_id] = GalleryPermission(gallery_id, gallery, true);
  dialog_->UpdateGalleries();
}

void MediaGalleriesDialogController::OnRemovableStorageAttached(
    const StorageInfo& info) {
  UpdateGalleriesOnDeviceEvent(info.device_id());
}

void MediaGalleriesDialogController::OnRemovableStorageDetached(
    const StorageInfo& info) {
  UpdateGalleriesOnDeviceEvent(info.device_id());
}

void MediaGalleriesDialogController::OnPermissionAdded(
    MediaGalleriesPreferences* /* prefs */,
    const std::string& extension_id,
    MediaGalleryPrefId /* pref_id */) {
  if (extension_id != extension_->id())
    return;
  UpdateGalleriesOnPreferencesEvent();
}

void MediaGalleriesDialogController::OnPermissionRemoved(
    MediaGalleriesPreferences* /* prefs */,
    const std::string& extension_id,
    MediaGalleryPrefId /* pref_id */) {
  if (extension_id != extension_->id())
    return;
  UpdateGalleriesOnPreferencesEvent();
}

void MediaGalleriesDialogController::OnGalleryAdded(
    MediaGalleriesPreferences* /* prefs */,
    MediaGalleryPrefId /* pref_id */) {
  UpdateGalleriesOnPreferencesEvent();
}

void MediaGalleriesDialogController::OnGalleryRemoved(
    MediaGalleriesPreferences* /* prefs */,
    MediaGalleryPrefId /* pref_id */) {
  UpdateGalleriesOnPreferencesEvent();
}

void MediaGalleriesDialogController::OnGalleryInfoUpdated(
    MediaGalleriesPreferences* prefs,
    MediaGalleryPrefId pref_id) {
  DCHECK(preferences_);
  const MediaGalleriesPrefInfoMap& pref_galleries =
      preferences_->known_galleries();
  MediaGalleriesPrefInfoMap::const_iterator pref_it =
      pref_galleries.find(pref_id);
  if (pref_it == pref_galleries.end())
    return;
  const MediaGalleryPrefInfo& gallery_info = pref_it->second;
  UpdateGalleriesOnDeviceEvent(gallery_info.device_id);
}

void MediaGalleriesDialogController::InitializePermissions() {
  known_galleries_.clear();
  DCHECK(preferences_);
  const MediaGalleriesPrefInfoMap& galleries = preferences_->known_galleries();
  for (MediaGalleriesPrefInfoMap::const_iterator iter = galleries.begin();
       iter != galleries.end();
       ++iter) {
    const MediaGalleryPrefInfo& gallery = iter->second;
    if (gallery.IsBlackListedType())
      continue;

    GalleryDialogId gallery_id = GetDialogId(gallery.pref_id);
    known_galleries_[gallery_id] =
        GalleryPermission(gallery_id, gallery, false);
  }

  MediaGalleryPrefIdSet permitted =
      preferences_->GalleriesForExtension(*extension_);

  for (MediaGalleryPrefIdSet::iterator iter = permitted.begin();
       iter != permitted.end(); ++iter) {
    GalleryDialogId gallery_id = GetDialogId(*iter);
    if (ContainsKey(toggled_galleries_, gallery_id))
      continue;
    DCHECK(ContainsKey(known_galleries_, gallery_id));
    known_galleries_[gallery_id].allowed = true;
  }
}

void MediaGalleriesDialogController::SavePermissions() {
  DCHECK(preferences_);
  media_galleries::UsageCount(media_galleries::SAVE_DIALOG);
  for (GalleryPermissionsMap::const_iterator iter = known_galleries_.begin();
       iter != known_galleries_.end(); ++iter) {
    MediaGalleryPrefId pref_id = iter->second.pref_info.pref_id;
    if (ContainsKey(forgotten_galleries_, iter->first)) {
      preferences_->ForgetGalleryById(pref_id);
    } else {
      bool changed = preferences_->SetGalleryPermissionForExtension(
          *extension_, pref_id, iter->second.allowed);
      if (changed) {
        if (iter->second.allowed) {
          media_galleries::UsageCount(
              media_galleries::DIALOG_PERMISSION_ADDED);
        } else {
          media_galleries::UsageCount(
              media_galleries::DIALOG_PERMISSION_REMOVED);
        }
      }
    }
  }

  for (GalleryPermissionsMap::const_iterator iter = new_galleries_.begin();
       iter != new_galleries_.end(); ++iter) {
    media_galleries::UsageCount(media_galleries::DIALOG_GALLERY_ADDED);
    // If the user added a gallery then unchecked it, forget about it.
    if (!iter->second.allowed)
      continue;

    const MediaGalleryPrefInfo& gallery = iter->second.pref_info;
    MediaGalleryPrefId id = preferences_->AddGallery(
        gallery.device_id, gallery.path, MediaGalleryPrefInfo::kUserAdded,
        gallery.volume_label, gallery.vendor_name, gallery.model_name,
        gallery.total_size_in_bytes, gallery.last_attach_time, 0, 0, 0);
    preferences_->SetGalleryPermissionForExtension(*extension_, id, true);
  }
}

void MediaGalleriesDialogController::UpdateGalleriesOnPreferencesEvent() {
  // Merge in the permissions from |preferences_|. Afterwards,
  // |known_galleries_| may contain galleries that no longer belong there,
  // but the code below will put |known_galleries_| back in a consistent state.
  InitializePermissions();

  std::set<GalleryDialogId> new_galleries_to_remove;
  // Look for duplicate entries in |new_galleries_| in case one was added
  // in another dialog.
  for (GalleryPermissionsMap::iterator it = known_galleries_.begin();
       it != known_galleries_.end();
       ++it) {
    GalleryPermission& gallery = it->second;
    for (GalleryPermissionsMap::iterator new_it = new_galleries_.begin();
         new_it != new_galleries_.end();
         ++new_it) {
      if (new_it->second.pref_info.path == gallery.pref_info.path &&
          new_it->second.pref_info.device_id == gallery.pref_info.device_id) {
        // Found duplicate entry. Get the existing permission from it and then
        // remove it.
        gallery.allowed = new_it->second.allowed;
        new_galleries_to_remove.insert(new_it->first);
        break;
      }
    }
  }
  for (std::set<GalleryDialogId>::const_iterator it =
           new_galleries_to_remove.begin();
       it != new_galleries_to_remove.end();
       ++it) {
    new_galleries_.erase(*it);
  }

  dialog_->UpdateGalleries();
}

void MediaGalleriesDialogController::UpdateGalleriesOnDeviceEvent(
    const std::string& device_id) {
  dialog_->UpdateGalleries();
}

ui::MenuModel* MediaGalleriesDialogController::GetContextMenu(
    GalleryDialogId gallery_id) {
  context_menu_->set_pref_id(gallery_id);
  return context_menu_.get();
}

GalleryDialogId MediaGalleriesDialogController::GetDialogId(
    MediaGalleryPrefId pref_id) {
  return id_map_.GetDialogId(pref_id);
}

Profile* MediaGalleriesDialogController::GetProfile() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

MediaGalleriesDialogController::DialogIdMap::DialogIdMap()
    : next_dialog_id_(1) {
}

MediaGalleriesDialogController::DialogIdMap::~DialogIdMap() {
}

GalleryDialogId
MediaGalleriesDialogController::DialogIdMap::GetDialogId(
    MediaGalleryPrefId pref_id) {
  std::map<GalleryDialogId, MediaGalleryPrefId>::const_iterator it =
      mapping_.find(pref_id);
  if (it != mapping_.end())
    return it->second;

  GalleryDialogId result = next_dialog_id_++;
  if (pref_id != kInvalidMediaGalleryPrefId)
    mapping_[pref_id] = result;
  return result;
}

// MediaGalleries dialog -------------------------------------------------------

MediaGalleriesDialog::~MediaGalleriesDialog() {}
