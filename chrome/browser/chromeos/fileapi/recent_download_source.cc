// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/recent_download_source.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/file_system_url.h"

using content::BrowserThread;

namespace chromeos {

namespace {

void OnReadDirectoryOnIOThread(
    const storage::FileSystemOperation::ReadDirectoryCallback& callback,
    base::File::Error result,
    const storage::FileSystemOperation::FileEntryList& entries,
    bool has_more) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(callback, result, entries, has_more));
}

void ReadDirectoryOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    const storage::FileSystemOperation::ReadDirectoryCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  file_system_context->operation_runner()->ReadDirectory(
      url, base::Bind(&OnReadDirectoryOnIOThread, callback));
}

void OnGetMetadataOnIOThread(
    const storage::FileSystemOperation::GetMetadataCallback& callback,
    base::File::Error result,
    const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(callback, result, info));
}

void GetMetadataOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    int fields,
    const storage::FileSystemOperation::GetMetadataCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  file_system_context->operation_runner()->GetMetadata(
      url, fields, base::Bind(&OnGetMetadataOnIOThread, callback));
}

}  // namespace

const char RecentDownloadSource::kLoadHistogramName[] =
    "FileBrowser.Recent.LoadDownloads";

RecentDownloadSource::RecentDownloadSource(Profile* profile)
    : mount_point_name_(
          file_manager::util::GetDownloadsMountPointName(profile)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RecentDownloadSource::~RecentDownloadSource() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void RecentDownloadSource::GetRecentFiles(RecentContext context,
                                          GetRecentFilesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!context_.is_valid());
  DCHECK(callback_.is_null());
  DCHECK(build_start_time_.is_null());
  DCHECK_EQ(0, inflight_readdirs_);
  DCHECK_EQ(0, inflight_stats_);
  DCHECK(recent_files_.empty());

  context_ = std::move(context);
  callback_ = std::move(callback);

  DCHECK(context_.is_valid());
  DCHECK(!callback_.is_null());

  build_start_time_ = base::TimeTicks::Now();

  ScanDirectory(base::FilePath());
}

void RecentDownloadSource::ScanDirectory(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_.is_valid());

  storage::FileSystemURL url = BuildDownloadsURL(path);

  ++inflight_readdirs_;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ReadDirectoryOnIOThread,
                     make_scoped_refptr(context_.file_system_context()), url,
                     base::Bind(&RecentDownloadSource::OnReadDirectory,
                                weak_ptr_factory_.GetWeakPtr(), path)));
}

void RecentDownloadSource::OnReadDirectory(
    const base::FilePath& path,
    base::File::Error result,
    const storage::FileSystemOperation::FileEntryList& entries,
    bool has_more) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_.is_valid());

  for (const auto& entry : entries) {
    base::FilePath subpath = path.Append(entry.name);
    if (entry.is_directory) {
      ScanDirectory(subpath);
    } else {
      storage::FileSystemURL url = BuildDownloadsURL(subpath);
      ++inflight_stats_;
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::BindOnce(
              &GetMetadataOnIOThread,
              make_scoped_refptr(context_.file_system_context()), url,
              storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
              base::Bind(&RecentDownloadSource::OnGetMetadata,
                         weak_ptr_factory_.GetWeakPtr(), url)));
    }
  }

  if (has_more)
    return;

  --inflight_readdirs_;
  OnReadOrStatFinished();
}

void RecentDownloadSource::OnGetMetadata(const storage::FileSystemURL& url,
                                         base::File::Error result,
                                         const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_.is_valid());

  if (result == base::File::FILE_OK &&
      info.last_modified >= context_.cutoff_time()) {
    recent_files_.emplace(RecentFile(url, info.last_modified));
    while (recent_files_.size() > context_.max_files())
      recent_files_.pop();
  }

  --inflight_stats_;
  OnReadOrStatFinished();
}

void RecentDownloadSource::OnReadOrStatFinished() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (inflight_readdirs_ > 0 || inflight_stats_ > 0)
    return;

  // All reads/scans completed.
  std::vector<RecentFile> files;
  while (!recent_files_.empty()) {
    files.emplace_back(recent_files_.top());
    recent_files_.pop();
  }

  DCHECK(!build_start_time_.is_null());
  UMA_HISTOGRAM_TIMES(kLoadHistogramName,
                      base::TimeTicks::Now() - build_start_time_);
  build_start_time_ = base::TimeTicks();

  context_ = RecentContext();
  GetRecentFilesCallback callback;
  std::swap(callback, callback_);

  DCHECK(!context_.is_valid());
  DCHECK(callback_.is_null());
  DCHECK(build_start_time_.is_null());
  DCHECK_EQ(0, inflight_readdirs_);
  DCHECK_EQ(0, inflight_stats_);
  DCHECK(recent_files_.empty());

  std::move(callback).Run(std::move(files));
}

storage::FileSystemURL RecentDownloadSource::BuildDownloadsURL(
    const base::FilePath& path) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_.is_valid());

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  return mount_points->CreateExternalFileSystemURL(context_.origin(),
                                                   mount_point_name_, path);
}

}  // namespace chromeos
