// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/local_file_sync_status.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "webkit/common/fileapi/file_system_util.h"

using storage::FileSystemURL;
using storage::FileSystemURLSet;

namespace sync_file_system {

namespace {

typedef LocalFileSyncStatus::OriginAndType OriginAndType;

OriginAndType GetOriginAndType(const storage::FileSystemURL& url) {
  return std::make_pair(url.origin(), url.type());
}

base::FilePath NormalizePath(const base::FilePath& path) {
  // Ensure |path| has single trailing path-separator, so that we can use
  // prefix-match to find descendants of |path| in an ordered container.
  return base::FilePath(path.StripTrailingSeparators().value() +
                        storage::VirtualPath::kSeparator);
}

struct SetKeyHelper {
  template <typename Iterator>
  static const base::FilePath& GetKey(Iterator itr) {
    return *itr;
  }
};

struct MapKeyHelper {
  template <typename Iterator>
  static const base::FilePath& GetKey(Iterator itr) {
    return itr->first;
  }
};

template <typename Container, typename GetKeyHelper>
bool ContainsChildOrParent(const Container& paths,
                           const base::FilePath& path,
                           const GetKeyHelper& get_key_helper) {
  base::FilePath normalized_path = NormalizePath(path);

  // Check if |paths| has a child of |normalized_path|.
  // Note that descendants of |normalized_path| are stored right after
  // |normalized_path| since |normalized_path| has trailing path separator.
  typename Container::const_iterator upper =
      paths.upper_bound(normalized_path);

  if (upper != paths.end() &&
      normalized_path.IsParent(get_key_helper.GetKey(upper)))
    return true;

  // Check if any ancestor of |normalized_path| is in |writing_|.
  while (true) {
    if (ContainsKey(paths, normalized_path))
      return true;

    if (storage::VirtualPath::IsRootPath(normalized_path))
      return false;

    normalized_path =
        NormalizePath(storage::VirtualPath::DirName(normalized_path));
  }
}

}  // namespace

LocalFileSyncStatus::LocalFileSyncStatus() {}

LocalFileSyncStatus::~LocalFileSyncStatus() {}

void LocalFileSyncStatus::StartWriting(const FileSystemURL& url) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsChildOrParentSyncing(url));
  writing_[GetOriginAndType(url)][NormalizePath(url.path())]++;
}

void LocalFileSyncStatus::EndWriting(const FileSystemURL& url) {
  DCHECK(CalledOnValidThread());
  base::FilePath normalized_path = NormalizePath(url.path());
  OriginAndType origin_and_type = GetOriginAndType(url);

  int count = --writing_[origin_and_type][normalized_path];
  if (count == 0) {
    writing_[origin_and_type].erase(normalized_path);
    if (writing_[origin_and_type].empty())
      writing_.erase(origin_and_type);
    FOR_EACH_OBSERVER(Observer, observer_list_, OnSyncEnabled(url));
  }
}

void LocalFileSyncStatus::StartSyncing(const FileSystemURL& url) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsChildOrParentWriting(url));
  DCHECK(!IsChildOrParentSyncing(url));
  syncing_[GetOriginAndType(url)].insert(NormalizePath(url.path()));
}

void LocalFileSyncStatus::EndSyncing(const FileSystemURL& url) {
  DCHECK(CalledOnValidThread());
  base::FilePath normalized_path = NormalizePath(url.path());
  OriginAndType origin_and_type = GetOriginAndType(url);

  syncing_[origin_and_type].erase(normalized_path);
  if (syncing_[origin_and_type].empty())
    syncing_.erase(origin_and_type);
  FOR_EACH_OBSERVER(Observer, observer_list_, OnSyncEnabled(url));
  FOR_EACH_OBSERVER(Observer, observer_list_, OnWriteEnabled(url));
}

bool LocalFileSyncStatus::IsWriting(const FileSystemURL& url) const {
  DCHECK(CalledOnValidThread());
  return IsChildOrParentWriting(url);
}

bool LocalFileSyncStatus::IsWritable(const FileSystemURL& url) const {
  DCHECK(CalledOnValidThread());
  return !IsChildOrParentSyncing(url);
}

bool LocalFileSyncStatus::IsSyncable(const FileSystemURL& url) const {
  DCHECK(CalledOnValidThread());
  return !IsChildOrParentSyncing(url) && !IsChildOrParentWriting(url);
}

void LocalFileSyncStatus::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void LocalFileSyncStatus::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

bool LocalFileSyncStatus::IsChildOrParentWriting(
    const FileSystemURL& url) const {
  DCHECK(CalledOnValidThread());

  URLBucket::const_iterator found = writing_.find(GetOriginAndType(url));
  if (found == writing_.end())
    return false;
  return ContainsChildOrParent(found->second, url.path(),
                               MapKeyHelper());
}

bool LocalFileSyncStatus::IsChildOrParentSyncing(
    const FileSystemURL& url) const {
  DCHECK(CalledOnValidThread());
  URLSet::const_iterator found = syncing_.find(GetOriginAndType(url));
  if (found == syncing_.end())
    return false;
  return ContainsChildOrParent(found->second, url.path(),
                               SetKeyHelper());
}

}  // namespace sync_file_system
