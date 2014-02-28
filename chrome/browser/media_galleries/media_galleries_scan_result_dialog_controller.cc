// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_scan_result_dialog_controller.h"

#include <algorithm>
#include <list>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_histograms.h"
#include "chrome/browser/media_galleries/media_gallery_context_menu.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/permissions/media_galleries_permission.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using storage_monitor::StorageInfo;
using storage_monitor::StorageMonitor;

namespace {

// Comparator for sorting OrderedScanResults -- more files first and then sorts
// by absolute path.
bool ScanResultsComparator(
    const MediaGalleriesScanResultDialogController::ScanResult& a,
    const MediaGalleriesScanResultDialogController::ScanResult& b) {
  int a_media_count = a.pref_info.audio_count + a.pref_info.image_count +
                      a.pref_info.video_count;
  int b_media_count = b.pref_info.audio_count + b.pref_info.image_count +
                      b.pref_info.video_count;
  if (a_media_count == b_media_count)
    return a.pref_info.AbsolutePath() < b.pref_info.AbsolutePath();
  return a_media_count > b_media_count;
}

}  // namespace

// static
size_t MediaGalleriesScanResultDialogController::ScanResultCountForExtension(
    MediaGalleriesPreferences* preferences,
    const extensions::Extension* extension) {
  ScanResults scan_results;
  UpdateScanResultsFromPreferences(preferences, extension,
                                   MediaGalleryPrefIdSet(), &scan_results);
  return scan_results.size();
}

MediaGalleriesScanResultDialogController::
MediaGalleriesScanResultDialogController(
    content::WebContents* web_contents,
    const extensions::Extension& extension,
    const base::Closure& on_finish)
      : web_contents_(web_contents),
        extension_(&extension),
        on_finish_(on_finish) {
  // TODO(vandebo): Put this in the intializer list after GTK is removed.
#if defined(USE_AURA) || defined(OS_MACOSX)
  create_dialog_callback_ = base::Bind(&MediaGalleriesScanResultDialog::Create);
#endif  // USE_AURA || OS_MACOSX
  preferences_ =
      g_browser_process->media_file_system_registry()->GetPreferences(
          GetProfile());
  // Passing unretained pointer is safe, since the dialog controller
  // is self-deleting, and so won't be deleted until it can be shown
  // and then closed.
  preferences_->EnsureInitialized(base::Bind(
        &MediaGalleriesScanResultDialogController::OnPreferencesInitialized,
        base::Unretained(this)));

  // Unretained is safe because |this| owns |context_menu_|.
  context_menu_.reset(new MediaGalleryContextMenu(base::Bind(
          &MediaGalleriesScanResultDialogController::DidForgetGallery,
          base::Unretained(this))));
}

MediaGalleriesScanResultDialogController::
MediaGalleriesScanResultDialogController(
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

MediaGalleriesScanResultDialogController::
~MediaGalleriesScanResultDialogController() {
  // |preferences_| may be NULL in tests.
  if (preferences_)
    preferences_->RemoveGalleryChangeObserver(this);
  if (StorageMonitor::GetInstance())
    StorageMonitor::GetInstance()->RemoveObserver(this);
}

base::string16 MediaGalleriesScanResultDialogController::GetHeader() const {
  return l10n_util::GetStringFUTF16(
      IDS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_HEADER,
      base::UTF8ToUTF16(extension_->name()));
}

base::string16 MediaGalleriesScanResultDialogController::GetSubtext() const {
  extensions::MediaGalleriesPermission::CheckParam copy_to_param(
      extensions::MediaGalleriesPermission::kCopyToPermission);
  extensions::MediaGalleriesPermission::CheckParam delete_param(
      extensions::MediaGalleriesPermission::kDeletePermission);
  bool has_copy_to_permission =
      extensions::PermissionsData::CheckAPIPermissionWithParam(
          extension_, extensions::APIPermission::kMediaGalleries,
          &copy_to_param);
  bool has_delete_permission =
      extensions::PermissionsData::CheckAPIPermissionWithParam(
          extension_, extensions::APIPermission::kMediaGalleries,
          &delete_param);

  int id;
  if (has_copy_to_permission)
    id = IDS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_SUBTEXT_READ_WRITE;
  else if (has_delete_permission)
    id = IDS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_SUBTEXT_READ_DELETE;
  else
    id = IDS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_SUBTEXT_READ_ONLY;

  return l10n_util::GetStringFUTF16(id, base::UTF8ToUTF16(extension_->name()));
}

MediaGalleriesScanResultDialogController::OrderedScanResults
MediaGalleriesScanResultDialogController::GetGalleryList() const {
  OrderedScanResults result;
  result.reserve(scan_results_.size());
  for (ScanResults::const_iterator it = scan_results_.begin();
       it != scan_results_.end();
       ++it) {
    result.push_back(it->second);
  }
  std::sort(result.begin(), result.end(), ScanResultsComparator);
  return result;
}

void MediaGalleriesScanResultDialogController::DidToggleGalleryId(
    MediaGalleryPrefId pref_id, bool selected) {
  DCHECK(ContainsKey(scan_results_, pref_id));
  ScanResults::iterator entry = scan_results_.find(pref_id);
  entry->second.selected = selected;
}

void MediaGalleriesScanResultDialogController::DidClickOpenFolderViewer(
    MediaGalleryPrefId pref_id) const {
  ScanResults::const_iterator entry = scan_results_.find(pref_id);
  if (entry == scan_results_.end()) {
    NOTREACHED();
    return;
  }
  platform_util::OpenItem(GetProfile(), entry->second.pref_info.AbsolutePath());
}

void MediaGalleriesScanResultDialogController::DidForgetGallery(
    MediaGalleryPrefId pref_id) {
  media_galleries::UsageCount(media_galleries::ADD_SCAN_RESULTS_FORGET_GALLERY);
  results_to_remove_.insert(pref_id);
  scan_results_.erase(pref_id);
  dialog_->UpdateResults();
}

void MediaGalleriesScanResultDialogController::DialogFinished(bool accepted) {
  // No longer interested in preference updates (and the below code generates
  // some).
  // |preferences_| may be NULL in tests.
  if (preferences_)
    preferences_->RemoveGalleryChangeObserver(this);

  if (accepted) {
    DCHECK(preferences_);
    media_galleries::UsageCount(media_galleries::ADD_SCAN_RESULTS_ACCEPTED);
    int granted = 0;
    int total = 0;
    for (ScanResults::const_iterator it = scan_results_.begin();
         it != scan_results_.end();
         ++it) {
      if (it->second.selected) {
        bool changed = preferences_->SetGalleryPermissionForExtension(
              *extension_, it->first, true);
        DCHECK(changed);
        granted++;
      }
      total++;
    }
    if (total > 0) {
      UMA_HISTOGRAM_PERCENTAGE("MediaGalleries.ScanGalleriesGranted",
                               (granted * 100 / total));
    }
    for (MediaGalleryPrefIdSet::const_iterator it = results_to_remove_.begin();
        it != results_to_remove_.end();
        ++it) {
      preferences_->ForgetGalleryById(*it);
    }
  } else {
    media_galleries::UsageCount(media_galleries::ADD_SCAN_RESULTS_CANCELLED);
  }

  on_finish_.Run();
  delete this;
}

content::WebContents* MediaGalleriesScanResultDialogController::web_contents() {
  return web_contents_;
}

ui::MenuModel* MediaGalleriesScanResultDialogController::GetContextMenu(
    MediaGalleryPrefId id) {
  context_menu_->set_pref_id(id);
  return context_menu_.get();
}

// static
void MediaGalleriesScanResultDialogController::UpdateScanResultsFromPreferences(
    MediaGalleriesPreferences* preferences,
    const extensions::Extension* extension,
    MediaGalleryPrefIdSet ignore_list, ScanResults* scan_results) {
  DCHECK(preferences->IsInitialized());
  const MediaGalleriesPrefInfoMap& galleries = preferences->known_galleries();
  MediaGalleryPrefIdSet permitted =
      preferences->GalleriesForExtension(*extension);

  // Add or update any scan results that the extension doesn't already have
  // access to or isn't in |ignore_list|.
  for (MediaGalleriesPrefInfoMap::const_iterator it = galleries.begin();
       it != galleries.end();
       ++it) {
    const MediaGalleryPrefInfo& gallery = it->second;
    if ((gallery.audio_count || gallery.image_count || gallery.video_count) &&
        !gallery.IsBlackListedType() &&
        !ContainsKey(permitted, gallery.pref_id) &&
        !ContainsKey(ignore_list, gallery.pref_id)) {
      ScanResults::iterator existing = scan_results->find(gallery.pref_id);
      if (existing == scan_results->end()) {
        // Default to selected.
        (*scan_results)[gallery.pref_id] = ScanResult(gallery, true);
      } else {
        // Update pref_info, in case anything has been updated.
        existing->second.pref_info = gallery;
      }
    }
  }

  // Remove anything from |scan_results| that's no longer valid or the user
  // already has access to.
  std::list<ScanResults::iterator> to_remove;
  for (ScanResults::iterator it = scan_results->begin();
       it != scan_results->end();
       ++it) {
    MediaGalleriesPrefInfoMap::const_iterator pref_gallery =
        galleries.find(it->first);
    if (pref_gallery == galleries.end() ||
        pref_gallery->second.IsBlackListedType() ||
        ContainsKey(permitted, it->first)) {
      to_remove.push_back(it);
    }
  }
  while (!to_remove.empty()) {
    scan_results->erase(to_remove.front());
    to_remove.pop_front();
  }
}

void MediaGalleriesScanResultDialogController::OnPreferencesInitialized() {
  // These may be NULL in tests.
  if (StorageMonitor::GetInstance())
    StorageMonitor::GetInstance()->AddObserver(this);
  if (preferences_) {
    preferences_->AddGalleryChangeObserver(this);
    UpdateScanResultsFromPreferences(preferences_, extension_,
                                     results_to_remove_, &scan_results_);
  }

  // TODO(vandebo): Remove the conditional after GTK is removed.
  if (!create_dialog_callback_.is_null())
    dialog_.reset(create_dialog_callback_.Run(this));
}

void MediaGalleriesScanResultDialogController::OnPreferenceUpdate(
    const std::string& extension_id) {
  if (extension_id == extension_->id()) {
    UpdateScanResultsFromPreferences(preferences_, extension_,
                                     results_to_remove_, &scan_results_);
    dialog_->UpdateResults();
  }
}

void MediaGalleriesScanResultDialogController::OnRemovableDeviceUpdate(
    const std::string device_id) {
  for (ScanResults::const_iterator it = scan_results_.begin();
       it != scan_results_.end();
       ++it) {
    if (it->second.pref_info.device_id == device_id) {
      dialog_->UpdateResults();
      return;
    }
  }
}

Profile* MediaGalleriesScanResultDialogController::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

void MediaGalleriesScanResultDialogController::OnRemovableStorageAttached(
    const StorageInfo& info) {
  OnRemovableDeviceUpdate(info.device_id());
}

void MediaGalleriesScanResultDialogController::OnRemovableStorageDetached(
    const StorageInfo& info) {
  OnRemovableDeviceUpdate(info.device_id());
}

void MediaGalleriesScanResultDialogController::OnPermissionAdded(
    MediaGalleriesPreferences* /*pref*/,
    const std::string& extension_id,
    MediaGalleryPrefId /*pref_id*/) {
  OnPreferenceUpdate(extension_id);
}

void MediaGalleriesScanResultDialogController::OnPermissionRemoved(
    MediaGalleriesPreferences* /*pref*/,
    const std::string& extension_id,
    MediaGalleryPrefId /*pref_id*/) {
  OnPreferenceUpdate(extension_id);
}

void MediaGalleriesScanResultDialogController::OnGalleryAdded(
    MediaGalleriesPreferences* /*prefs*/,
    MediaGalleryPrefId /*pref_id*/) {
  OnPreferenceUpdate(extension_->id());
}

void MediaGalleriesScanResultDialogController::OnGalleryRemoved(
    MediaGalleriesPreferences* /*prefs*/,
    MediaGalleryPrefId /*pref_id*/) {
  OnPreferenceUpdate(extension_->id());
}

void MediaGalleriesScanResultDialogController::OnGalleryInfoUpdated(
    MediaGalleriesPreferences* /*prefs*/,
    MediaGalleryPrefId /*pref_id*/) {
  OnPreferenceUpdate(extension_->id());
}

// MediaGalleriesScanResultDialog ---------------------------------------------

MediaGalleriesScanResultDialog::~MediaGalleriesScanResultDialog() {}
