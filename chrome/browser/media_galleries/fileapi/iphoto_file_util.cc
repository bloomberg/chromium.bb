// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/iphoto_file_util.h"

#include <set>
#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media_galleries/fileapi/iphoto_data_provider.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/native_file_util.h"
#include "webkit/common/blob/shareable_file_reference.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace iphoto {

namespace {

base::PlatformFileError MakeDirectoryFileInfo(
    base::PlatformFileInfo* file_info) {
  base::PlatformFileInfo result;
  result.is_directory = true;
  *file_info = result;
  return base::PLATFORM_FILE_OK;
}

}  // namespace

IPhotoFileUtil::IPhotoFileUtil(MediaPathFilter* media_path_filter)
    : NativeMediaFileUtil(media_path_filter),
      weak_factory_(this),
      imported_registry_(NULL) {
}

IPhotoFileUtil::~IPhotoFileUtil() {
}

void IPhotoFileUtil::GetFileInfoOnTaskRunnerThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  GetDataProvider()->RefreshData(
      base::Bind(&IPhotoFileUtil::GetFileInfoWithFreshDataProvider,
                 weak_factory_.GetWeakPtr(), base::Passed(&context), url,
                 callback));
}

void IPhotoFileUtil::ReadDirectoryOnTaskRunnerThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  GetDataProvider()->RefreshData(
      base::Bind(&IPhotoFileUtil::ReadDirectoryWithFreshDataProvider,
                 weak_factory_.GetWeakPtr(), base::Passed(&context), url,
                 callback));
}

void IPhotoFileUtil::CreateSnapshotFileOnTaskRunnerThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  GetDataProvider()->RefreshData(
      base::Bind(&IPhotoFileUtil::CreateSnapshotFileWithFreshDataProvider,
                 weak_factory_.GetWeakPtr(), base::Passed(&context), url,
                 callback));
}

void IPhotoFileUtil::GetFileInfoWithFreshDataProvider(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const GetFileInfoCallback& callback,
    bool valid_parse) {
  if (!valid_parse) {
    if (!callback.is_null()) {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(callback, base::PLATFORM_FILE_ERROR_IO,
                     base::PlatformFileInfo()));
    }
    return;
  }
  NativeMediaFileUtil::GetFileInfoOnTaskRunnerThread(context.Pass(), url,
                                                     callback);
}

void IPhotoFileUtil::ReadDirectoryWithFreshDataProvider(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const ReadDirectoryCallback& callback,
    bool valid_parse) {
  if (!valid_parse) {
    if (!callback.is_null()) {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(callback, base::PLATFORM_FILE_ERROR_IO, EntryList(),
                     false));
    }
    return;
  }
  NativeMediaFileUtil::ReadDirectoryOnTaskRunnerThread(context.Pass(), url,
                                                       callback);
}

void IPhotoFileUtil::CreateSnapshotFileWithFreshDataProvider(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback,
    bool valid_parse) {
  if (!valid_parse) {
    if (!callback.is_null()) {
      base::PlatformFileInfo file_info;
      base::FilePath platform_path;
      scoped_refptr<webkit_blob::ShareableFileReference> file_ref;
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(callback, base::PLATFORM_FILE_ERROR_IO, file_info,
                     platform_path, file_ref));
    }
    return;
  }
  NativeMediaFileUtil::CreateSnapshotFileOnTaskRunnerThread(context.Pass(), url,
                                                            callback);
}

// TODO(gbillock): Right now, returns an always-empty FS. Document virtual FS.
base::PlatformFileError IPhotoFileUtil::GetFileInfoSync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    base::PlatformFileInfo* file_info,
    base::FilePath* platform_path) {
  std::vector<std::string> components;
  fileapi::VirtualPath::GetComponentsUTF8Unsafe(url.path(), &components);

  if (components.size() == 0)
    return MakeDirectoryFileInfo(file_info);

  return base::PLATFORM_FILE_ERROR_NOT_FOUND;
}

base::PlatformFileError IPhotoFileUtil::ReadDirectorySync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    EntryList* file_list) {
  DCHECK(file_list->empty());
  std::vector<std::string> components;
  fileapi::VirtualPath::GetComponentsUTF8Unsafe(url.path(), &components);

  if (components.size() == 0) {
    return base::PLATFORM_FILE_OK;
  }

  return base::PLATFORM_FILE_ERROR_NOT_FOUND;
}

base::PlatformFileError IPhotoFileUtil::DeleteDirectorySync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

base::PlatformFileError IPhotoFileUtil::DeleteFileSync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

base::PlatformFileError IPhotoFileUtil::CreateSnapshotFileSync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    base::PlatformFileInfo* file_info,
    base::FilePath* platform_path,
    scoped_refptr<webkit_blob::ShareableFileReference>* file_ref) {
  DCHECK(!url.path().IsAbsolute());

  return base::PLATFORM_FILE_ERROR_NOT_FOUND;
}

base::PlatformFileError IPhotoFileUtil::GetLocalFilePath(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    base::FilePath* local_file_path) {
  return base::PLATFORM_FILE_ERROR_NOT_FOUND;
}

IPhotoDataProvider* IPhotoFileUtil::GetDataProvider() {
  if (!imported_registry_)
    imported_registry_ = ImportedMediaGalleryRegistry::GetInstance();
  return imported_registry_->IPhotoDataProvider();
}

}  // namespace iphoto
