// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_quota_util.h"
#include "storage/common/fileapi/file_system_types.h"

using content::BrowserThread;

namespace storage {
class FileSystemContext;
}

namespace {

// An implementation of the BrowsingDataFileSystemHelper interface that pulls
// data from a given |filesystem_context| and returns a list of FileSystemInfo
// items to a client.
class BrowsingDataFileSystemHelperImpl : public BrowsingDataFileSystemHelper {
 public:
  // BrowsingDataFileSystemHelper implementation
  explicit BrowsingDataFileSystemHelperImpl(
      storage::FileSystemContext* filesystem_context);
  void StartFetching(FetchCallback callback) override;
  void DeleteFileSystemOrigin(const url::Origin& origin) override;

 private:
  ~BrowsingDataFileSystemHelperImpl() override;

  // Enumerates all filesystem files, storing the resulting list into
  // file_system_file_ for later use. This must be called on the file
  // task runner.
  void FetchFileSystemInfoInFileThread(FetchCallback callback);

  // Deletes all file systems associated with |origin|. This must be called on
  // the file task runner.
  void DeleteFileSystemOriginInFileThread(const url::Origin& origin);

  // Returns the file task runner for the |filesystem_context_|.
  base::SequencedTaskRunner* file_task_runner() {
    return filesystem_context_->default_file_task_runner();
  }

  // Keep a reference to the FileSystemContext object for the current profile
  // for use on the file task runner.
  scoped_refptr<storage::FileSystemContext> filesystem_context_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataFileSystemHelperImpl);
};

BrowsingDataFileSystemHelperImpl::BrowsingDataFileSystemHelperImpl(
    storage::FileSystemContext* filesystem_context)
    : filesystem_context_(filesystem_context) {
  DCHECK(filesystem_context_.get());
}

BrowsingDataFileSystemHelperImpl::~BrowsingDataFileSystemHelperImpl() {
}

void BrowsingDataFileSystemHelperImpl::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  file_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BrowsingDataFileSystemHelperImpl::FetchFileSystemInfoInFileThread,
          this, std::move(callback)));
}

void BrowsingDataFileSystemHelperImpl::DeleteFileSystemOrigin(
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BrowsingDataFileSystemHelperImpl::DeleteFileSystemOriginInFileThread,
          this, origin));
}

void BrowsingDataFileSystemHelperImpl::FetchFileSystemInfoInFileThread(
    FetchCallback callback) {
  DCHECK(file_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!callback.is_null());

  // We check usage for these filesystem types.
  const storage::FileSystemType types[] = {
    storage::kFileSystemTypeTemporary,
    storage::kFileSystemTypePersistent,
#if BUILDFLAG(ENABLE_EXTENSIONS)
    storage::kFileSystemTypeSyncable,
#endif
  };

  std::list<FileSystemInfo> result;
  std::map<GURL, FileSystemInfo> file_system_info_map;
  for (size_t i = 0; i < base::size(types); ++i) {
    storage::FileSystemType type = types[i];
    storage::FileSystemQuotaUtil* quota_util =
        filesystem_context_->GetQuotaUtil(type);
    DCHECK(quota_util);
    std::set<GURL> origins;
    quota_util->GetOriginsForTypeOnFileTaskRunner(type, &origins);
    for (const GURL& current : origins) {
      if (!BrowsingDataHelper::HasWebScheme(current))
        continue;  // Non-websafe state is not considered browsing data.
      int64_t usage = quota_util->GetOriginUsageOnFileTaskRunner(
          filesystem_context_.get(), current, type);
      auto inserted =
          file_system_info_map
              .insert(std::make_pair(
                  current, FileSystemInfo(url::Origin::Create(current))))
              .first;
      inserted->second.usage_map[type] = usage;
    }
  }

  for (const auto& iter : file_system_info_map)
    result.push_back(iter.second);

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), result));
}

void BrowsingDataFileSystemHelperImpl::DeleteFileSystemOriginInFileThread(
    const url::Origin& origin) {
  DCHECK(file_task_runner()->RunsTasksInCurrentSequence());
  filesystem_context_->DeleteDataForOriginOnFileTaskRunner(origin.GetURL());
}

}  // namespace

BrowsingDataFileSystemHelper::FileSystemInfo::FileSystemInfo(
    const url::Origin& origin)
    : origin(origin) {}

BrowsingDataFileSystemHelper::FileSystemInfo::FileSystemInfo(
    const FileSystemInfo& other) = default;

BrowsingDataFileSystemHelper::FileSystemInfo::~FileSystemInfo() {}

// static
BrowsingDataFileSystemHelper* BrowsingDataFileSystemHelper::Create(
    storage::FileSystemContext* filesystem_context) {
  return new BrowsingDataFileSystemHelperImpl(filesystem_context);
}

CannedBrowsingDataFileSystemHelper::CannedBrowsingDataFileSystemHelper(
    Profile* profile) {
}

CannedBrowsingDataFileSystemHelper::~CannedBrowsingDataFileSystemHelper() {}

void CannedBrowsingDataFileSystemHelper::Add(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!BrowsingDataHelper::HasWebScheme(origin.GetURL()))
    return;  // Non-websafe state is not considered browsing data.
  pending_origins_.insert(origin);
}

void CannedBrowsingDataFileSystemHelper::Reset() {
  pending_origins_.clear();
}

bool CannedBrowsingDataFileSystemHelper::empty() const {
  return pending_origins_.empty();
}

size_t CannedBrowsingDataFileSystemHelper::GetCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pending_origins_.size();
}

void CannedBrowsingDataFileSystemHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<FileSystemInfo> result;
  for (const auto& origin : pending_origins_)
    result.emplace_back(origin);

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), result));
}
