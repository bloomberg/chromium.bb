// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/media_galleries/fileapi/picasa/picasa_data_provider.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/isolated_context.h"

using base::Bind;

namespace chrome {

namespace {

bool CurrentlyOnMediaTaskRunnerThread() {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken media_sequence_token =
      pool->GetNamedSequenceToken(fileapi::kMediaTaskRunnerName);

  return pool->IsRunningSequenceOnCurrentThread(media_sequence_token);
}

scoped_refptr<base::SequencedTaskRunner> MediaTaskRunner() {
  DCHECK(!CurrentlyOnMediaTaskRunnerThread());
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken media_sequence_token =
      pool->GetNamedSequenceToken(fileapi::kMediaTaskRunnerName);

  return pool->GetSequencedTaskRunner(media_sequence_token);
}

static base::LazyInstance<ImportedMediaGalleryRegistry>::Leaky
g_imported_media_gallery_registry = LAZY_INSTANCE_INITIALIZER;

}

// static
ImportedMediaGalleryRegistry* ImportedMediaGalleryRegistry::GetInstance() {
  return g_imported_media_gallery_registry.Pointer();
}

// static
std::string ImportedMediaGalleryRegistry::RegisterPicasaFilesystemOnUIThread(
    const base::FilePath& database_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::string fsid =
      fileapi::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
           fileapi::kFileSystemTypePicasa,
           extension_misc::kMediaFileSystemPathPart,
           base::FilePath());

  if (fsid.empty())
    return "";

  MediaTaskRunner()->PostTask(
      FROM_HERE,
      Bind(&ImportedMediaGalleryRegistry::RegisterPicasaFileSystem,
           base::Unretained(GetInstance()), database_path));

  return fsid;
}

// static
bool ImportedMediaGalleryRegistry::RevokePicasaFilesystemOnUIThread(
    const std::string& fsid) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!fileapi::IsolatedContext::GetInstance()->RevokeFileSystem(fsid))
    return false;

  MediaTaskRunner()->PostTask(
      FROM_HERE,
      Bind(&ImportedMediaGalleryRegistry::RevokePicasaFileSystem,
           base::Unretained(GetInstance())));

  return true;
}

// static
picasa::PicasaDataProvider*
ImportedMediaGalleryRegistry::picasa_data_provider() {
  DCHECK(CurrentlyOnMediaTaskRunnerThread());
  DCHECK(GetInstance()->picasa_data_provider_);
  return GetInstance()->picasa_data_provider_.get();
}

ImportedMediaGalleryRegistry::ImportedMediaGalleryRegistry()
    : picasa_filesystems_count_(0) {
}

ImportedMediaGalleryRegistry::~ImportedMediaGalleryRegistry() {
  DCHECK_EQ(0, picasa_filesystems_count_);
}

void ImportedMediaGalleryRegistry::RegisterPicasaFileSystem(
    const base::FilePath& database_path) {
  DCHECK(CurrentlyOnMediaTaskRunnerThread());

  if (++picasa_filesystems_count_ == 1) {
    DCHECK(!picasa_data_provider_);
    picasa_data_provider_.reset(new picasa::PicasaDataProvider(database_path));
  }
}

void ImportedMediaGalleryRegistry::RevokePicasaFileSystem() {
  DCHECK(CurrentlyOnMediaTaskRunnerThread());

  if (--picasa_filesystems_count_ == 0) {
    DCHECK(picasa_data_provider_);
    picasa_data_provider_.reset(NULL);
  }
}

}  // namespace chrome
