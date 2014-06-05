// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_scan_manager.h"

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"
#include "chrome/browser/media_galleries/media_scan_manager_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/media_galleries.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"

using extensions::ExtensionRegistry;

namespace media_galleries = extensions::api::media_galleries;

namespace {

typedef std::set<std::string /*extension id*/> ScanningExtensionIdSet;

// When multiple scan results have the same parent, sometimes it makes sense
// to combine them into a single scan result at the parent. This constant
// governs when that happens; kContainerDirectoryMinimumPercent percent of the
// directories in the parent directory must be scan results.
const int kContainerDirectoryMinimumPercent = 80;

// How long after a completed media scan can we provide the cached results.
const int kScanResultsExpiryTimeInHours = 24;

struct LocationInfo {
  LocationInfo()
      : pref_id(kInvalidMediaGalleryPrefId),
        type(MediaGalleryPrefInfo::kInvalidType) {}
  LocationInfo(MediaGalleryPrefId pref_id, MediaGalleryPrefInfo::Type type,
               base::FilePath path)
      : pref_id(pref_id), type(type), path(path) {}
  // Highest priority comparison by path, next by type (scan result last),
  // then by pref id (invalid last).
  bool operator<(const LocationInfo& rhs) const {
    if (path.value() == rhs.path.value()) {
      if (type == rhs.type) {
        return pref_id > rhs.pref_id;
      }
      return rhs.type == MediaGalleryPrefInfo::kScanResult;
    }
    return path.value() < rhs.path.value();
  }

  MediaGalleryPrefId pref_id;
  MediaGalleryPrefInfo::Type type;
  base::FilePath path;
  MediaGalleryScanResult file_counts;
};

// Finds new scan results that are shadowed (the same location, or a child) by
// existing locations and moves them from |found_folders| to |child_folders|.
// Also moves new scan results that are shadowed by other new scan results
// to |child_folders|.
void PartitionChildScanResults(
    MediaGalleriesPreferences* preferences,
    MediaFolderFinder::MediaFolderFinderResults* found_folders,
    MediaFolderFinder::MediaFolderFinderResults* child_folders) {
  // Construct a list with everything in it.
  std::vector<LocationInfo> all_locations;
  for (MediaFolderFinder::MediaFolderFinderResults::const_iterator it =
           found_folders->begin(); it != found_folders->end(); ++it) {
    all_locations.push_back(LocationInfo(kInvalidMediaGalleryPrefId,
                                         MediaGalleryPrefInfo::kScanResult,
                                         it->first));
    all_locations.back().file_counts = it->second;
  }
  const MediaGalleriesPrefInfoMap& known_galleries =
      preferences->known_galleries();
  for (MediaGalleriesPrefInfoMap::const_iterator it = known_galleries.begin();
       it != known_galleries.end();
       ++it) {
    all_locations.push_back(LocationInfo(it->second.pref_id, it->second.type,
                                         it->second.AbsolutePath()));
  }
  // Sorting on path should put all paths that are prefixes of other paths
  // next to each other, with the shortest one first.
  std::sort(all_locations.begin(), all_locations.end());

  size_t previous_parent_index = 0;
  for (size_t i = 1; i < all_locations.size(); i++) {
    const LocationInfo& current = all_locations[i];
    const LocationInfo& previous_parent = all_locations[previous_parent_index];
    bool is_child = previous_parent.path.IsParent(current.path);
    if (current.type == MediaGalleryPrefInfo::kScanResult &&
        current.pref_id == kInvalidMediaGalleryPrefId &&
        (is_child || previous_parent.path == current.path)) {
      // Move new scan results that are shadowed.
      (*child_folders)[current.path] = current.file_counts;
      found_folders->erase(current.path);
    } else if (!is_child) {
      previous_parent_index = i;
    }
  }
}

MediaGalleryScanResult SumFilesUnderPath(
    const base::FilePath& path,
    const MediaFolderFinder::MediaFolderFinderResults& candidates) {
  MediaGalleryScanResult results;
  for (MediaFolderFinder::MediaFolderFinderResults::const_iterator it =
           candidates.begin(); it != candidates.end(); ++it) {
    if (it->first == path || path.IsParent(it->first)) {
      results.audio_count += it->second.audio_count;
      results.image_count += it->second.image_count;
      results.video_count += it->second.video_count;
    }
  }
  return results;
}

void AddScanResultsForProfile(
    MediaGalleriesPreferences* preferences,
    const MediaFolderFinder::MediaFolderFinderResults& found_folders) {
  // First, remove any existing scan results where no app has been granted
  // permission - either it is gone, or is already in the new scan results.
  // This burns some pref ids, but not at an appreciable rate.
  MediaGalleryPrefIdSet to_remove;
  const MediaGalleriesPrefInfoMap& known_galleries =
      preferences->known_galleries();
  for (MediaGalleriesPrefInfoMap::const_iterator it = known_galleries.begin();
       it != known_galleries.end();
       ++it) {
    if (it->second.type == MediaGalleryPrefInfo::kScanResult &&
        !preferences->NonAutoGalleryHasPermission(it->first)) {
      to_remove.insert(it->first);
    }
  }
  for (MediaGalleryPrefIdSet::const_iterator it = to_remove.begin();
       it != to_remove.end();
       ++it) {
    preferences->EraseGalleryById(*it);
  }

  MediaFolderFinder::MediaFolderFinderResults child_folders;
  MediaFolderFinder::MediaFolderFinderResults
      unique_found_folders(found_folders);
  PartitionChildScanResults(preferences, &unique_found_folders, &child_folders);

  // Updating prefs while iterating them will invalidate the pointer, so
  // calculate the changes first and then apply them.
  std::map<MediaGalleryPrefId, MediaGalleryScanResult> to_update;
  for (MediaGalleriesPrefInfoMap::const_iterator it = known_galleries.begin();
       it != known_galleries.end();
       ++it) {
    const MediaGalleryPrefInfo& gallery = it->second;
    if (!gallery.IsBlackListedType()) {
      MediaGalleryScanResult file_counts =
          SumFilesUnderPath(gallery.AbsolutePath(), child_folders);
      if (gallery.audio_count != file_counts.audio_count ||
          gallery.image_count != file_counts.image_count ||
          gallery.video_count != file_counts.video_count) {
        to_update[it->first] = file_counts;
      }
    }
  }

  for (std::map<MediaGalleryPrefId,
                MediaGalleryScanResult>::const_iterator it = to_update.begin();
       it != to_update.end();
       ++it) {
    const MediaGalleryPrefInfo& gallery =
        preferences->known_galleries().find(it->first)->second;
      preferences->AddGallery(gallery.device_id, gallery.path, gallery.type,
                              gallery.volume_label, gallery.vendor_name,
                              gallery.model_name, gallery.total_size_in_bytes,
                              gallery.last_attach_time,
                              it->second.audio_count,
                              it->second.image_count,
                              it->second.video_count);
  }

  // Add new scan results.
  for (MediaFolderFinder::MediaFolderFinderResults::const_iterator it =
           unique_found_folders.begin();
       it != unique_found_folders.end();
       ++it) {
    MediaGalleryScanResult file_counts =
        SumFilesUnderPath(it->first, child_folders);
    // The top level scan result is not in |child_folders|. Add it in as well.
    file_counts.audio_count += it->second.audio_count;
    file_counts.image_count += it->second.image_count;
    file_counts.video_count += it->second.video_count;

    MediaGalleryPrefInfo gallery;
    bool existing = preferences->LookUpGalleryByPath(it->first, &gallery);
    DCHECK(!existing);
    preferences->AddGallery(gallery.device_id, gallery.path,
                            MediaGalleryPrefInfo::kScanResult,
                            gallery.volume_label, gallery.vendor_name,
                            gallery.model_name, gallery.total_size_in_bytes,
                            gallery.last_attach_time, file_counts.audio_count,
                            file_counts.image_count, file_counts.video_count);
  }
  UMA_HISTOGRAM_COUNTS_10000("MediaGalleries.ScanGalleriesPopulated",
                             unique_found_folders.size() + to_update.size());
}

int CountScanResultsForExtension(MediaGalleriesPreferences* preferences,
                                 const extensions::Extension* extension,
                                 MediaGalleryScanResult* file_counts) {
  int gallery_count = 0;

  MediaGalleryPrefIdSet permitted_galleries =
      preferences->GalleriesForExtension(*extension);
  const MediaGalleriesPrefInfoMap& known_galleries =
      preferences->known_galleries();
  for (MediaGalleriesPrefInfoMap::const_iterator it = known_galleries.begin();
       it != known_galleries.end();
       ++it) {
    if (it->second.type == MediaGalleryPrefInfo::kScanResult &&
        !ContainsKey(permitted_galleries, it->first)) {
      gallery_count++;
      file_counts->audio_count += it->second.audio_count;
      file_counts->image_count += it->second.image_count;
      file_counts->video_count += it->second.video_count;
    }
  }
  return gallery_count;
}

int CountDirectoryEntries(const base::FilePath& path) {
  base::FileEnumerator dir_counter(
      path, false /*recursive*/, base::FileEnumerator::DIRECTORIES);
  int count = 0;
  base::FileEnumerator::FileInfo info;
  for (base::FilePath name = dir_counter.Next(); !name.empty();
       name = dir_counter.Next()) {
    if (!base::IsLink(name))
      ++count;
  }
  return count;
}

struct ContainerCount {
  int seen_count, entries_count;
  bool is_qualified;

  ContainerCount() : seen_count(0), entries_count(-1), is_qualified(false) {}
};

typedef std::map<base::FilePath, ContainerCount> ContainerCandidates;

}  // namespace

MediaScanManager::MediaScanManager()
    : scoped_extension_registry_observer_(this),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

MediaScanManager::~MediaScanManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void MediaScanManager::AddObserver(Profile* profile,
                                   MediaScanManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!ContainsKey(observers_, profile));
  observers_[profile].observer = observer;
}

void MediaScanManager::RemoveObserver(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool scan_in_progress = ScanInProgress();
  observers_.erase(profile);
  DCHECK_EQ(scan_in_progress, ScanInProgress());
}

void MediaScanManager::CancelScansForProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observers_[profile].scanning_extensions.clear();

  if (!ScanInProgress())
    folder_finder_.reset();
}

void MediaScanManager::StartScan(Profile* profile,
                                 const extensions::Extension* extension,
                                 bool user_gesture) {
  DCHECK(extension);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ScanObserverMap::iterator scans_for_profile = observers_.find(profile);
  // We expect that an MediaScanManagerObserver has already been registered.
  DCHECK(scans_for_profile != observers_.end());
  bool scan_in_progress = ScanInProgress();
  // Ignore requests for extensions that are already scanning.
  ScanningExtensionIdSet* scanning_extensions;
  scanning_extensions = &scans_for_profile->second.scanning_extensions;
  if (scan_in_progress && ContainsKey(*scanning_extensions, extension->id()))
    return;

  // Provide cached result if there is not already a scan in progress,
  // there is no user gesture, and the previous results are unexpired.
  MediaGalleriesPreferences* preferences =
      MediaGalleriesPreferencesFactory::GetForProfile(profile);
  base::TimeDelta time_since_last_scan =
      base::Time::Now() - preferences->GetLastScanCompletionTime();
  if (!scan_in_progress && !user_gesture && time_since_last_scan <
          base::TimeDelta::FromHours(kScanResultsExpiryTimeInHours)) {
    MediaGalleryScanResult file_counts;
    int gallery_count =
        CountScanResultsForExtension(preferences, extension, &file_counts);
    scans_for_profile->second.observer->OnScanStarted(extension->id());
    scans_for_profile->second.observer->OnScanFinished(extension->id(),
                                                       gallery_count,
                                                       file_counts);
    return;
  }

  // On first scan for the |profile|, register to listen for extension unload.
  if (scanning_extensions->empty())
    scoped_extension_registry_observer_.Add(ExtensionRegistry::Get(profile));

  scanning_extensions->insert(extension->id());
  scans_for_profile->second.observer->OnScanStarted(extension->id());

  if (folder_finder_)
    return;

  MediaFolderFinder::MediaFolderFinderResultsCallback callback =
      base::Bind(&MediaScanManager::OnScanCompleted,
                 weak_factory_.GetWeakPtr());
  if (testing_folder_finder_factory_.is_null()) {
    folder_finder_.reset(new MediaFolderFinder(callback));
  } else {
    folder_finder_.reset(testing_folder_finder_factory_.Run(callback));
  }
  scan_start_time_ = base::Time::Now();
  folder_finder_->StartScan();
}

void MediaScanManager::CancelScan(Profile* profile,
                                  const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Erases the logical scan if found, early exit otherwise.
  ScanObserverMap::iterator scans_for_profile = observers_.find(profile);
  if (scans_for_profile == observers_.end() ||
      !scans_for_profile->second.scanning_extensions.erase(extension->id())) {
    return;
  }

  scans_for_profile->second.observer->OnScanCancelled(extension->id());

  // No more scanning extensions for |profile|, so stop listening for unloads.
  if (scans_for_profile->second.scanning_extensions.empty())
    scoped_extension_registry_observer_.Remove(ExtensionRegistry::Get(profile));

  if (!ScanInProgress()) {
    folder_finder_.reset();
    DCHECK(!scan_start_time_.is_null());
    UMA_HISTOGRAM_LONG_TIMES("MediaGalleries.ScanCancelTime",
                             base::Time::Now() - scan_start_time_);
    scan_start_time_ = base::Time();
  }
}

void MediaScanManager::SetMediaFolderFinderFactory(
    const MediaFolderFinderFactory& factory) {
  testing_folder_finder_factory_ = factory;
}

// A single directory may contain many folders with media in them, without
// containing any media itself. In fact, the primary purpose of that directory
// may be to contain media directories. This function tries to find those
// container directories.
MediaFolderFinder::MediaFolderFinderResults
MediaScanManager::FindContainerScanResults(
    const MediaFolderFinder::MediaFolderFinderResults& found_folders,
    const std::vector<base::FilePath>& sensitive_locations) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
  std::vector<base::FilePath> abs_sensitive_locations;
  for (size_t i = 0; i < sensitive_locations.size(); ++i) {
    base::FilePath path = base::MakeAbsoluteFilePath(sensitive_locations[i]);
    if (!path.empty())
      abs_sensitive_locations.push_back(path);
  }
  // Recursively find parent directories with majority of media directories,
  // or container directories.
  // |candidates| keeps track of directories which might have enough
  // such directories to have us return them.
  typedef std::map<base::FilePath, ContainerCount> ContainerCandidates;
  ContainerCandidates candidates;
  for (MediaFolderFinder::MediaFolderFinderResults::const_iterator it =
           found_folders.begin();
       it != found_folders.end();
       ++it) {
    base::FilePath child_directory = it->first;
    base::FilePath parent_directory = child_directory.DirName();

    // Parent of root is root.
    while (!parent_directory.empty() && child_directory != parent_directory) {
      // Skip sensitive folders and their ancestors.
      base::FilePath abs_parent_directory =
          base::MakeAbsoluteFilePath(parent_directory);
      if (abs_parent_directory.empty())
        break;
      bool is_sensitive = false;
      for (size_t i = 0; i < abs_sensitive_locations.size(); ++i) {
        if (abs_parent_directory == abs_sensitive_locations[i] ||
            abs_parent_directory.IsParent(abs_sensitive_locations[i])) {
          is_sensitive = true;
          break;
        }
      }
      if (is_sensitive)
        break;

      // Don't bother with ones we already have.
      if (found_folders.find(parent_directory) != found_folders.end())
        continue;

      ContainerCandidates::iterator parent_it =
          candidates.find(parent_directory);
      if (parent_it == candidates.end()) {
        ContainerCount count;
        count.seen_count = 1;
        count.entries_count = CountDirectoryEntries(parent_directory);
        parent_it =
            candidates.insert(std::make_pair(parent_directory, count)).first;
      } else {
        ++candidates[parent_directory].seen_count;
      }
      // If previously sufficient, or not sufficient, bail.
      if (parent_it->second.is_qualified ||
          parent_it->second.seen_count * 100 / parent_it->second.entries_count <
              kContainerDirectoryMinimumPercent) {
        break;
      }
      // Otherwise, mark qualified and check parent.
      parent_it->second.is_qualified = true;
      child_directory = parent_directory;
      parent_directory = child_directory.DirName();
    }
  }
  MediaFolderFinder::MediaFolderFinderResults result;
  // Copy and return worthy results.
  for (ContainerCandidates::const_iterator it = candidates.begin();
       it != candidates.end();
       ++it) {
    if (it->second.is_qualified && it->second.seen_count >= 2)
      result[it->first] = MediaGalleryScanResult();
  }
  return result;
}

MediaScanManager::ScanObservers::ScanObservers() : observer(NULL) {}
MediaScanManager::ScanObservers::~ScanObservers() {}

void MediaScanManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CancelScan(Profile::FromBrowserContext(browser_context), extension);
}

bool MediaScanManager::ScanInProgress() const {
  for (ScanObserverMap::const_iterator it = observers_.begin();
       it != observers_.end();
       ++it) {
    if (!it->second.scanning_extensions.empty())
      return true;
  }
  return false;
}

void MediaScanManager::OnScanCompleted(
    bool success,
    const MediaFolderFinder::MediaFolderFinderResults& found_folders) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!folder_finder_ || !success) {
    folder_finder_.reset();
    return;
  }

  UMA_HISTOGRAM_COUNTS_10000("MediaGalleries.ScanDirectoriesFound",
                             found_folders.size());
  DCHECK(!scan_start_time_.is_null());
  UMA_HISTOGRAM_LONG_TIMES("MediaGalleries.ScanFinishedTime",
                           base::Time::Now() - scan_start_time_);
  scan_start_time_ = base::Time();

  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(FindContainerScanResults,
                 found_folders,
                 folder_finder_->graylisted_folders()),
      base::Bind(&MediaScanManager::OnFoundContainerDirectories,
                 weak_factory_.GetWeakPtr(),
                 found_folders));
}

void MediaScanManager::OnFoundContainerDirectories(
    const MediaFolderFinder::MediaFolderFinderResults& found_folders,
    const MediaFolderFinder::MediaFolderFinderResults& container_folders) {
  MediaFolderFinder::MediaFolderFinderResults folders;
  folders.insert(found_folders.begin(), found_folders.end());
  folders.insert(container_folders.begin(), container_folders.end());

  for (ScanObserverMap::iterator scans_for_profile = observers_.begin();
       scans_for_profile != observers_.end();
       ++scans_for_profile) {
    if (scans_for_profile->second.scanning_extensions.empty())
      continue;
    Profile* profile = scans_for_profile->first;
    MediaGalleriesPreferences* preferences =
        MediaGalleriesPreferencesFactory::GetForProfile(profile);
    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    if (!extension_service)
      continue;

    AddScanResultsForProfile(preferences, folders);

    ScanningExtensionIdSet* scanning_extensions =
        &scans_for_profile->second.scanning_extensions;
    for (ScanningExtensionIdSet::const_iterator extension_id_it =
             scanning_extensions->begin();
         extension_id_it != scanning_extensions->end();
         ++extension_id_it) {
      const extensions::Extension* extension =
          extension_service->GetExtensionById(*extension_id_it, false);
      if (extension) {
        MediaGalleryScanResult file_counts;
        int gallery_count = CountScanResultsForExtension(preferences, extension,
                                                         &file_counts);
        scans_for_profile->second.observer->OnScanFinished(*extension_id_it,
                                                           gallery_count,
                                                           file_counts);
      }
    }
    scanning_extensions->clear();
    preferences->SetLastScanCompletionTime(base::Time::Now());
  }
  scoped_extension_registry_observer_.RemoveAll();
  folder_finder_.reset();
}
