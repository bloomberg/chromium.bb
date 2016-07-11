// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_downloads_watcher_service.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "components/arc/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/array.h"

using content::BrowserThread;

// Mapping from Android file paths to last modified timestamps.
using TimestampMap = std::map<base::FilePath, base::Time>;

namespace arc {

namespace {

const base::FilePath::CharType kAndroidDownloadDir[] =
    FILE_PATH_LITERAL("/storage/emulated/0/Download");

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

}  // namespace

// The core part of ArcDownloadsWatcherService to watch for file changes in
// Downloads directory.
class ArcDownloadsWatcherService::DownloadsWatcher {
 public:
  using Callback =
      base::Callback<void(const std::vector<base::FilePath>& paths)>;

  explicit DownloadsWatcher(const Callback& callback);
  ~DownloadsWatcher();

  // Starts watching Downloads directory.
  void Start();

 private:
  // Called by base::FilePathWatcher to notify file changes.
  void OnFilePathChanged(const base::FilePath& path, bool error);

  // Scans files under |downloads_dir_| recursively and builds a map from file
  // paths (in Android filesystem) to last modified timestamps.
  TimestampMap BuildTimestampMap() const;

  Callback callback_;
  base::FilePath downloads_dir_;
  std::unique_ptr<base::FilePathWatcher> watcher_;
  TimestampMap last_timestamp_map_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DownloadsWatcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsWatcher);
};

ArcDownloadsWatcherService::DownloadsWatcher::DownloadsWatcher(
    const Callback& callback)
    : callback_(callback), weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  downloads_dir_ = DownloadPrefs(ProfileManager::GetActiveUserProfile())
                       .GetDefaultDownloadDirectoryForProfile()
                       .StripTrailingSeparators();
}

ArcDownloadsWatcherService::DownloadsWatcher::~DownloadsWatcher() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
}

void ArcDownloadsWatcherService::DownloadsWatcher::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  // Initialize with the current timestamp map and avoid initial notification.
  // It is not needed since MediaProvider scans whole storage area on boot.
  last_timestamp_map_ = BuildTimestampMap();

  watcher_ = base::MakeUnique<base::FilePathWatcher>();
  // On Linux, base::FilePathWatcher::Watch() always returns true.
  watcher_->Watch(downloads_dir_, true,
                  base::Bind(&DownloadsWatcher::OnFilePathChanged,
                             weak_ptr_factory_.GetWeakPtr()));
}

void ArcDownloadsWatcherService::DownloadsWatcher::OnFilePathChanged(
    const base::FilePath& path,
    bool error) {
  // On Linux, |error| is always false. Also, |path| is always the same path
  // as one given to FilePathWatcher::Watch().
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  TimestampMap current_timestamp_map = BuildTimestampMap();

  std::vector<base::FilePath> changed_paths =
      CollectChangedPaths(last_timestamp_map_, current_timestamp_map);

  last_timestamp_map_ = std::move(current_timestamp_map);

  callback_.Run(changed_paths);
}

TimestampMap ArcDownloadsWatcherService::DownloadsWatcher::BuildTimestampMap()
    const {
  DCHECK(!downloads_dir_.EndsWithSeparator());
  TimestampMap timestamp_map;

  // Enumerate normal files only; directories and symlinks are skipped.
  base::FileEnumerator enumerator(downloads_dir_, true,
                                  base::FileEnumerator::FILES);
  for (base::FilePath cros_path = enumerator.Next(); !cros_path.empty();
       cros_path = enumerator.Next()) {
    // Android file path can be obtained by replacing |downloads_dir_| prefix
    // with |kAndroidDownloadDir|.
    const base::FilePath& android_path =
        base::FilePath(kAndroidDownloadDir)
            .Append(
                cros_path.value().substr(downloads_dir_.value().length() + 1));
    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();
    timestamp_map[android_path] = info.GetLastModifiedTime();
  }
  return timestamp_map;
}

ArcDownloadsWatcherService::ArcDownloadsWatcherService(
    ArcBridgeService* bridge_service)
    : ArcService(bridge_service), weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  arc_bridge_service()->AddObserver(this);
}

ArcDownloadsWatcherService::~ArcDownloadsWatcherService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  arc_bridge_service()->RemoveObserver(this);
  StopWatchingDownloads();
  DCHECK(!watcher_);
}

void ArcDownloadsWatcherService::OnFileSystemInstanceReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StartWatchingDownloads();
}

void ArcDownloadsWatcherService::OnFileSystemInstanceClosed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StopWatchingDownloads();
}

void ArcDownloadsWatcherService::StartWatchingDownloads() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StopWatchingDownloads();
  DCHECK(!watcher_);
  watcher_ = base::MakeUnique<DownloadsWatcher>(
      base::Bind(&ArcDownloadsWatcherService::OnDownloadsChanged,
                 weak_ptr_factory_.GetWeakPtr()));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadsWatcher::Start, base::Unretained(watcher_.get())));
}

void ArcDownloadsWatcherService::StopWatchingDownloads() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (watcher_) {
    BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE,
                              watcher_.release());
  }
}

void ArcDownloadsWatcherService::OnDownloadsChanged(
    const std::vector<base::FilePath>& paths) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  auto* instance = arc_bridge_service()->file_system_instance();
  if (!instance)
    return;

  mojo::Array<mojo::String> mojo_paths(paths.size());
  for (size_t i = 0; i < paths.size(); ++i) {
    mojo_paths[i] = paths[i].value();
  }
  instance->RequestMediaScan(std::move(mojo_paths));
}

}  // namespace arc
