// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/file_system_watcher/arc_file_system_watcher_service.h"

#include <string.h>

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/singleton.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/file_system_watcher/arc_file_system_watcher_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// Mapping from Android file paths to last modified timestamps.
using TimestampMap = std::map<base::FilePath, base::Time>;

namespace arc {

namespace {

// The Downloads path inside ARC container. This will be the path that
// is used in MediaScanner.scanFile request.
constexpr base::FilePath::CharType kAndroidDownloadDir[] =
    FILE_PATH_LITERAL("/storage/emulated/0/Download");

// The MyFiles path inside ARC container. This will be the path that is used in
// MediaScanner.scanFile request.
constexpr base::FilePath::CharType kAndroidMyFilesDir[] =
    FILE_PATH_LITERAL("/storage/MyFiles");

// The path for Downloads under MyFiles inside ARC container.
constexpr base::FilePath::CharType kAndroidMyFilesDownloadsDir[] =
    FILE_PATH_LITERAL("/storage/MyFiles/Downloads/");

// The removable media path inside ARC container. This will be the path that
// is used in MediaScanner.scanFile request.
constexpr base::FilePath::CharType kAndroidRemovableMediaDir[] =
    FILE_PATH_LITERAL("/storage");

// How long to wait for new inotify events before building the updated timestamp
// map.
const base::TimeDelta kBuildTimestampMapDelay =
    base::TimeDelta::FromMilliseconds(1000);

// Compares two TimestampMaps and returns the list of file paths added/removed
// or whose timestamp have changed.
std::vector<base::FilePath> CollectChangedPaths(
    const TimestampMap& timestamp_map_a,
    const TimestampMap& timestamp_map_b) {
  std::vector<base::FilePath> changed_paths;

  TimestampMap::const_iterator iter_a = timestamp_map_a.begin();
  TimestampMap::const_iterator iter_b = timestamp_map_b.begin();
  while (iter_a != timestamp_map_a.end() && iter_b != timestamp_map_b.end()) {
    if (iter_a->first == iter_b->first) {
      if (iter_a->second != iter_b->second) {
        changed_paths.emplace_back(iter_a->first);
      }
      ++iter_a;
      ++iter_b;
    } else if (iter_a->first < iter_b->first) {
      changed_paths.emplace_back(iter_a->first);
      ++iter_a;
    } else {  // iter_a->first > iter_b->first
      changed_paths.emplace_back(iter_b->first);
      ++iter_b;
    }
  }

  while (iter_a != timestamp_map_a.end()) {
    changed_paths.emplace_back(iter_a->first);
    ++iter_a;
  }
  while (iter_b != timestamp_map_b.end()) {
    changed_paths.emplace_back(iter_b->first);
    ++iter_b;
  }

  return changed_paths;
}

// Scans files under |cros_dir| recursively and builds a map from
// file paths (in Android filesystem) to last modified timestamps.
TimestampMap BuildTimestampMap(base::FilePath cros_dir,
                               base::FilePath android_dir) {
  DCHECK(!cros_dir.EndsWithSeparator());
  TimestampMap timestamp_map;

  // Enumerate normal files only; directories and symlinks are skipped.
  base::FileEnumerator enumerator(cros_dir, true,
                                  base::FileEnumerator::FILES);
  for (base::FilePath cros_path = enumerator.Next(); !cros_path.empty();
       cros_path = enumerator.Next()) {
    // Skip non-media files for efficiency.
    if (!HasAndroidSupportedMediaExtension(cros_path))
      continue;
    base::FilePath android_path =
        GetAndroidPath(cros_path, cros_dir, android_dir);
    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();
    timestamp_map[android_path] = info.GetLastModifiedTime();
  }
  return timestamp_map;
}

std::pair<base::TimeTicks, TimestampMap> BuildTimestampMapCallback(
    base::FilePath cros_dir,
    base::FilePath android_dir) {
  // The TimestampMap may include changes form after snapshot_time.
  // We must take the snapshot_time before we build the TimestampMap since
  // changes that occur while building the map may not be captured.
  base::TimeTicks snapshot_time = base::TimeTicks::Now();
  TimestampMap current_timestamp_map =
      BuildTimestampMap(cros_dir, android_dir);
  return std::make_pair(snapshot_time, std::move(current_timestamp_map));
}

// Singleton factory for ArcFileSystemWatcherService.
class ArcFileSystemWatcherServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcFileSystemWatcherService,
          ArcFileSystemWatcherServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcFileSystemWatcherServiceFactory";

  static ArcFileSystemWatcherServiceFactory* GetInstance() {
    return base::Singleton<ArcFileSystemWatcherServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcFileSystemWatcherServiceFactory>;
  ArcFileSystemWatcherServiceFactory() = default;
  ~ArcFileSystemWatcherServiceFactory() override = default;
};

}  // namespace

// The core part of ArcFileSystemWatcherService to watch for file changes in
// directory.
class ArcFileSystemWatcherService::FileSystemWatcher {
 public:
  using Callback = base::Callback<void(const std::vector<std::string>& paths)>;

  FileSystemWatcher(content::BrowserContext* context,
                    const Callback& callback,
                    const base::FilePath& cros_dir,
                    const base::FilePath& android_dir);
  ~FileSystemWatcher();

  // Starts watching directory.
  void Start();

 private:
  // Called by base::FilePathWatcher to notify file changes.
  // Kicks off the update of last_timestamp_map_ if one is not already in
  // progress.
  void OnFilePathChanged(const base::FilePath& path, bool error);

  // Called with a delay to allow additional inotify events for the same user
  // action to queue up so that they can be dealt with in batch.
  void DelayBuildTimestampMap();

  // Called after a new timestamp map has been created and causes any recently
  // modified files to be sent to the media scanner.
  void OnBuildTimestampMap(
      std::pair<base::TimeTicks, TimestampMap> timestamp_and_map);

  Callback callback_;
  const base::FilePath cros_dir_;
  const base::FilePath android_dir_;
  std::unique_ptr<base::FilePathWatcher> watcher_;
  TimestampMap last_timestamp_map_;
  // The timestamp of the last OnFilePathChanged callback received.
  base::TimeTicks last_notify_time_;
  // Whether or not there is an outstanding task to update last_timestamp_map_.
  bool outstanding_task_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileSystemWatcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FileSystemWatcher);
};

ArcFileSystemWatcherService::FileSystemWatcher::FileSystemWatcher(
    content::BrowserContext* context,
    const Callback& callback,
    const base::FilePath& cros_dir,
    const base::FilePath& android_dir)
    : callback_(callback),
      cros_dir_(cros_dir),
      android_dir_(android_dir),
      last_notify_time_(base::TimeTicks()),
      outstanding_task_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ArcFileSystemWatcherService::FileSystemWatcher::~FileSystemWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ArcFileSystemWatcherService::FileSystemWatcher::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Initialize with the current timestamp map and avoid initial notification.
  // It is not needed since MediaProvider scans whole storage area on boot.
  last_notify_time_ = base::TimeTicks::Now();
  last_timestamp_map_ =
      BuildTimestampMap(cros_dir_, android_dir_);

  watcher_ = std::make_unique<base::FilePathWatcher>();
  // On Linux, base::FilePathWatcher::Watch() always returns true.
  watcher_->Watch(cros_dir_, true,
                  base::Bind(&FileSystemWatcher::OnFilePathChanged,
                             weak_ptr_factory_.GetWeakPtr()));
}

void ArcFileSystemWatcherService::FileSystemWatcher::OnFilePathChanged(
    const base::FilePath& path,
    bool error) {
  // On Linux, |error| is always false. Also, |path| is always the same path
  // as one given to FilePathWatcher::Watch().
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!outstanding_task_) {
    outstanding_task_ = true;
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FileSystemWatcher::DelayBuildTimestampMap,
                       weak_ptr_factory_.GetWeakPtr()),
        kBuildTimestampMapDelay);
  } else {
    last_notify_time_ = base::TimeTicks::Now();
  }
}

void ArcFileSystemWatcherService::FileSystemWatcher::DelayBuildTimestampMap() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(outstanding_task_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&BuildTimestampMapCallback, cros_dir_, android_dir_),
      base::BindOnce(&FileSystemWatcher::OnBuildTimestampMap,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcFileSystemWatcherService::FileSystemWatcher::OnBuildTimestampMap(
    std::pair<base::TimeTicks, TimestampMap> timestamp_and_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(outstanding_task_);
  base::TimeTicks snapshot_time = timestamp_and_map.first;
  TimestampMap current_timestamp_map = std::move(timestamp_and_map.second);
  std::vector<base::FilePath> changed_paths =
      CollectChangedPaths(last_timestamp_map_, current_timestamp_map);

  last_timestamp_map_ = std::move(current_timestamp_map);

  std::vector<std::string> string_paths(changed_paths.size());
  for (size_t i = 0; i < changed_paths.size(); ++i) {
    string_paths[i] = changed_paths[i].value();
  }
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(callback_, std::move(string_paths)));
  if (last_notify_time_ > snapshot_time)
    DelayBuildTimestampMap();
  else
    outstanding_task_ = false;
}

// static
ArcFileSystemWatcherService* ArcFileSystemWatcherService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcFileSystemWatcherServiceFactory::GetForBrowserContext(context);
}

ArcFileSystemWatcherService::ArcFileSystemWatcherService(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : context_(context),
      arc_bridge_service_(bridge_service),
      file_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  arc_bridge_service_->file_system()->AddObserver(this);
}

ArcFileSystemWatcherService::~ArcFileSystemWatcherService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StopWatchingFileSystem();
  DCHECK(!downloads_watcher_);
  DCHECK(!myfiles_watcher_);
  DCHECK(!removable_media_watcher_);

  arc_bridge_service_->file_system()->RemoveObserver(this);
}

void ArcFileSystemWatcherService::OnConnectionReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StartWatchingFileSystem();
}

void ArcFileSystemWatcherService::OnConnectionClosed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StopWatchingFileSystem();
}

void ArcFileSystemWatcherService::StartWatchingFileSystem() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StopWatchingFileSystem();
  Profile* profile = Profile::FromBrowserContext(context_);

  DCHECK(!downloads_watcher_);
  downloads_watcher_ = CreateAndStartFileSystemWatcher(
      DownloadPrefs(profile)
          .GetDefaultDownloadDirectoryForProfile()
          .StripTrailingSeparators(),
      base::FilePath(kAndroidDownloadDir));

  DCHECK(!myfiles_watcher_);
  myfiles_watcher_ = CreateAndStartFileSystemWatcher(
      file_manager::util::GetMyFilesFolderForProfile(profile),
      base::FilePath(kAndroidMyFilesDir));

  DCHECK(!removable_media_watcher_);
  removable_media_watcher_ = CreateAndStartFileSystemWatcher(
      base::FilePath(kCrosRemovableMediaDir),
      base::FilePath(kAndroidRemovableMediaDir));
}

void ArcFileSystemWatcherService::StopWatchingFileSystem() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (downloads_watcher_)
    file_task_runner_->DeleteSoon(FROM_HERE, downloads_watcher_.release());
  if (myfiles_watcher_)
    file_task_runner_->DeleteSoon(FROM_HERE, myfiles_watcher_.release());
  if (removable_media_watcher_)
    file_task_runner_->DeleteSoon(FROM_HERE,
                                  removable_media_watcher_.release());
}

std::unique_ptr<ArcFileSystemWatcherService::FileSystemWatcher>
ArcFileSystemWatcherService::CreateAndStartFileSystemWatcher(
    const base::FilePath& cros_path,
    const base::FilePath& android_path) {
  auto watcher = std::make_unique<FileSystemWatcher>(
      context_,
      base::BindRepeating(&ArcFileSystemWatcherService::OnFileSystemChanged,
                          weak_ptr_factory_.GetWeakPtr()),
      cros_path, android_path);
  file_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&FileSystemWatcher::Start,
                                             base::Unretained(watcher.get())));
  return watcher;
}

void ArcFileSystemWatcherService::OnFileSystemChanged(
    const std::vector<std::string>& paths) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->file_system(), RequestMediaScan);
  if (!instance)
    return;

  std::vector<std::string> filtered_paths;
  for (const std::string& path : paths) {
    if (base::StartsWith(path, kAndroidMyFilesDownloadsDir,
                         base::CompareCase::SENSITIVE)) {
      // Exclude files under /storage/MyFiles/Downloads/ because they are also
      // indexed as files under /storage/emulated/0/Download/
      continue;
    }
    filtered_paths.push_back(path);
  }

  instance->RequestMediaScan(filtered_paths);
}

}  // namespace arc
