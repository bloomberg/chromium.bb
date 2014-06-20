// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/snapshot_manager.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/task_util.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/common/blob/shareable_file_reference.h"

namespace file_manager {
namespace {

// Part of CreateManagedSnapshot. Runs CreateSnapshotFile method of fileapi.
void CreateSnapshotFileOnIOThread(
    scoped_refptr<fileapi::FileSystemContext> context,
    const fileapi::FileSystemURL& url,
    const fileapi::FileSystemOperation::SnapshotFileCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  context->operation_runner()->CreateSnapshotFile(url, callback);
}

// Utility for destructing the bound |file_refs| on IO thread. This is meant
// to be used together with base::Bind. After this function finishes, the
// Bind callback should destruct the bound argument.
void FreeReferenceOnIOThread(const std::vector<
    scoped_refptr<webkit_blob::ShareableFileReference> >& file_refs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

}  // namespace

SnapshotManager::SnapshotManager(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
}

SnapshotManager::~SnapshotManager() {
  if (!file_refs_.empty()) {
    bool posted = content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&FreeReferenceOnIOThread, file_refs_));
    DCHECK(posted);
  }
}

void SnapshotManager::CreateManagedSnapshot(
    const base::FilePath& absolute_file_path,
    const LocalPathCallback& callback) {
  fileapi::FileSystemContext* context =
      util::GetFileSystemContextForExtensionId(profile_, kFileManagerAppId);
  DCHECK(context);

  GURL url;
  if (!util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile_, absolute_file_path, kFileManagerAppId, &url)) {
    callback.Run(base::FilePath());
    return;
  }

  // TODO(kinaba): crbug.com/386519 evict old |file_refs_| before creating a new
  // one if necessary.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CreateSnapshotFileOnIOThread,
                 make_scoped_refptr(context),
                 context->CrackURL(url),
                 google_apis::CreateRelayCallback(
                     base::Bind(&SnapshotManager::OnCreateSnapshotFile,
                                weak_ptr_factory_.GetWeakPtr(),
                                callback))));
}

void SnapshotManager::OnCreateSnapshotFile(
    const LocalPathCallback& callback,
    base::File::Error result,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != base::File::FILE_OK) {
    callback.Run(base::FilePath());
    return;
  }

  file_refs_.push_back(file_ref);
  callback.Run(platform_path);
}

}  // namespace file_manager
