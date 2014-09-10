// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/iphoto_file_util.h"

#include <set>
#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media_galleries/fileapi/iphoto_data_provider.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/native_file_util.h"
#include "storage/common/blob/shareable_file_reference.h"
#include "storage/common/fileapi/directory_entry.h"
#include "storage/common/fileapi/file_system_util.h"

using storage::DirectoryEntry;

namespace iphoto {

namespace {

base::File::Error MakeDirectoryFileInfo(base::File::Info* file_info) {
  base::File::Info result;
  result.is_directory = true;
  *file_info = result;
  return base::File::FILE_OK;
}

template <typename T>
bool ContainsElement(const std::vector<T>& collection, const T& key) {
  typename std::vector<T>::const_iterator it = collection.begin();
  while (it != collection.end()) {
    if (*it == key)
      return true;
    it++;
  }
  return false;
}

std::vector<std::string> GetVirtualPathComponents(
    const storage::FileSystemURL& url) {
  ImportedMediaGalleryRegistry* imported_registry =
      ImportedMediaGalleryRegistry::GetInstance();
  base::FilePath root = imported_registry->ImportedRoot().AppendASCII("iphoto");

  DCHECK(root.IsParent(url.path()) || root == url.path());
  base::FilePath virtual_path;
  root.AppendRelativePath(url.path(), &virtual_path);

  std::vector<std::string> result;
  storage::VirtualPath::GetComponentsUTF8Unsafe(virtual_path, &result);
  return result;
}

}  // namespace

const char kIPhotoAlbumsDir[] = "Albums";
const char kIPhotoOriginalsDir[] = "Originals";

IPhotoFileUtil::IPhotoFileUtil(MediaPathFilter* media_path_filter)
    : NativeMediaFileUtil(media_path_filter),
      weak_factory_(this),
      imported_registry_(NULL) {
}

IPhotoFileUtil::~IPhotoFileUtil() {
}

void IPhotoFileUtil::GetFileInfoOnTaskRunnerThread(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  IPhotoDataProvider* data_provider = GetDataProvider();
  // |data_provider| may be NULL if the file system was revoked before this
  // operation had a chance to run.
  if (!data_provider) {
    GetFileInfoWithFreshDataProvider(context.Pass(), url, callback, false);
  } else {
    data_provider->RefreshData(
        base::Bind(&IPhotoFileUtil::GetFileInfoWithFreshDataProvider,
                   weak_factory_.GetWeakPtr(), base::Passed(&context), url,
                   callback));
  }
}

void IPhotoFileUtil::ReadDirectoryOnTaskRunnerThread(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  IPhotoDataProvider* data_provider = GetDataProvider();
  // |data_provider| may be NULL if the file system was revoked before this
  // operation had a chance to run.
  if (!data_provider) {
    ReadDirectoryWithFreshDataProvider(context.Pass(), url, callback, false);
  } else {
    data_provider->RefreshData(
        base::Bind(&IPhotoFileUtil::ReadDirectoryWithFreshDataProvider,
                   weak_factory_.GetWeakPtr(), base::Passed(&context), url,
                   callback));
  }
}

void IPhotoFileUtil::CreateSnapshotFileOnTaskRunnerThread(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  IPhotoDataProvider* data_provider = GetDataProvider();
  // |data_provider| may be NULL if the file system was revoked before this
  // operation had a chance to run.
  if (!data_provider) {
    CreateSnapshotFileWithFreshDataProvider(context.Pass(), url, callback,
                                            false);
  } else {
    data_provider->RefreshData(
        base::Bind(&IPhotoFileUtil::CreateSnapshotFileWithFreshDataProvider,
                   weak_factory_.GetWeakPtr(), base::Passed(&context), url,
                   callback));
  }
}

void IPhotoFileUtil::GetFileInfoWithFreshDataProvider(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const GetFileInfoCallback& callback,
    bool valid_parse) {
  if (!valid_parse) {
    if (!callback.is_null()) {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(callback, base::File::FILE_ERROR_IO, base::File::Info()));
    }
    return;
  }
  NativeMediaFileUtil::GetFileInfoOnTaskRunnerThread(context.Pass(), url,
                                                     callback);
}

void IPhotoFileUtil::ReadDirectoryWithFreshDataProvider(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const ReadDirectoryCallback& callback,
    bool valid_parse) {
  if (!valid_parse) {
    if (!callback.is_null()) {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(callback, base::File::FILE_ERROR_IO, EntryList(), false));
    }
    return;
  }
  NativeMediaFileUtil::ReadDirectoryOnTaskRunnerThread(context.Pass(), url,
                                                       callback);
}

void IPhotoFileUtil::CreateSnapshotFileWithFreshDataProvider(
    scoped_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback,
    bool valid_parse) {
  if (!valid_parse) {
    if (!callback.is_null()) {
      base::File::Info file_info;
      base::FilePath platform_path;
      scoped_refptr<storage::ShareableFileReference> file_ref;
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(callback, base::File::FILE_ERROR_IO, file_info,
                     platform_path, file_ref));
    }
    return;
  }
  NativeMediaFileUtil::CreateSnapshotFileOnTaskRunnerThread(context.Pass(), url,
                                                            callback);
}

// Begin actual implementation.

base::File::Error IPhotoFileUtil::GetFileInfoSync(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url,
    base::File::Info* file_info,
    base::FilePath* platform_path) {
  std::vector<std::string> components = GetVirtualPathComponents(url);

  if (components.size() == 0) {
    return MakeDirectoryFileInfo(file_info);
  }

  // The 'Albums' directory.
  if (components[0] == kIPhotoAlbumsDir) {
    if (components.size() == 1) {
      return MakeDirectoryFileInfo(file_info);
    } else if (components.size() == 2) {
      std::vector<std::string> albums =
          GetDataProvider()->GetAlbumNames();
      if (ContainsElement(albums, components[1]))
        return MakeDirectoryFileInfo(file_info);
    } else if (components.size() == 3) {
      if (components[2] == kIPhotoOriginalsDir) {
        if (GetDataProvider()->HasOriginals(components[1]))
          return MakeDirectoryFileInfo(file_info);
        else
          return base::File::FILE_ERROR_NOT_FOUND;
      }

      base::FilePath location = GetDataProvider()->GetPhotoLocationInAlbum(
          components[1], components[2]);
      if (!location.empty()) {
        return NativeMediaFileUtil::GetFileInfoSync(
            context, url, file_info, platform_path);
      }
    } else if (components.size() == 4 &&
               GetDataProvider()->HasOriginals(components[1]) &&
               components[2] == kIPhotoOriginalsDir) {
      base::FilePath location = GetDataProvider()->GetOriginalPhotoLocation(
          components[1], components[3]);
      if (!location.empty()) {
        return NativeMediaFileUtil::GetFileInfoSync(
            context, url, file_info, platform_path);
      }
    }
  }

  return base::File::FILE_ERROR_NOT_FOUND;
}

base::File::Error IPhotoFileUtil::ReadDirectorySync(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url,
    EntryList* file_list) {
  DCHECK(file_list->empty());
  std::vector<std::string> components = GetVirtualPathComponents(url);

  // Root directory. Child is the /Albums dir.
  if (components.size() == 0) {
    file_list->push_back(DirectoryEntry(kIPhotoAlbumsDir,
                                        DirectoryEntry::DIRECTORY,
                                        0, base::Time()));
    return base::File::FILE_OK;
  }

  if (components[0] == kIPhotoAlbumsDir) {
    if (components.size() == 1) {
      // Albums dir contains all album names.
      std::vector<std::string> albums =
          GetDataProvider()->GetAlbumNames();
      for (std::vector<std::string>::const_iterator it = albums.begin();
           it != albums.end(); it++) {
        file_list->push_back(DirectoryEntry(*it, DirectoryEntry::DIRECTORY,
                                            0, base::Time()));
      }
      return base::File::FILE_OK;
    } else if (components.size() == 2) {
      std::vector<std::string> albums =
          GetDataProvider()->GetAlbumNames();
      if (!ContainsElement(albums, components[1]))
        return base::File::FILE_ERROR_NOT_FOUND;

      // Album dirs contain all photos in them.
      if (GetDataProvider()->HasOriginals(components[1])) {
        file_list->push_back(DirectoryEntry(kIPhotoOriginalsDir,
                                            DirectoryEntry::DIRECTORY,
                                            0, base::Time()));
      }
      std::map<std::string, base::FilePath> locations =
          GetDataProvider()->GetAlbumContents(components[1]);
      for (std::map<std::string, base::FilePath>::const_iterator it =
               locations.begin();
           it != locations.end(); it++) {
        base::File::Info info;
        if (!base::GetFileInfo(it->second, &info))
          return base::File::FILE_ERROR_IO;
        file_list->push_back(DirectoryEntry(it->first, DirectoryEntry::FILE,
                                            info.size, info.last_modified));
      }
      return base::File::FILE_OK;
    } else if (components.size() == 3 &&
               components[2] == kIPhotoOriginalsDir &&
               GetDataProvider()->HasOriginals(components[1])) {
      std::map<std::string, base::FilePath> originals =
          GetDataProvider()->GetOriginals(components[1]);
      for (std::map<std::string, base::FilePath>::const_iterator it =
               originals.begin();
           it != originals.end(); it++) {
        base::File::Info info;
        if (!base::GetFileInfo(it->second, &info))
          return base::File::FILE_ERROR_IO;
        file_list->push_back(DirectoryEntry(it->first, DirectoryEntry::FILE,
                                            info.size, info.last_modified));
      }
      return base::File::FILE_OK;
    }
  }

  return base::File::FILE_ERROR_NOT_FOUND;
}

base::File::Error IPhotoFileUtil::DeleteDirectorySync(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url) {
  return base::File::FILE_ERROR_SECURITY;
}

base::File::Error IPhotoFileUtil::DeleteFileSync(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url) {
  return base::File::FILE_ERROR_SECURITY;
}

base::File::Error IPhotoFileUtil::GetLocalFilePath(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url,
    base::FilePath* local_file_path) {
  std::vector<std::string> components = GetVirtualPathComponents(url);

  if (components.size() == 3 && components[0] == kIPhotoAlbumsDir) {
    base::FilePath location = GetDataProvider()->GetPhotoLocationInAlbum(
        components[1], components[2]);
    if (!location.empty()) {
      *local_file_path = location;
      return base::File::FILE_OK;
    }
  }

  if (components.size() == 4 && components[0] == kIPhotoAlbumsDir &&
      GetDataProvider()->HasOriginals(components[1]) &&
      components[2] == kIPhotoOriginalsDir) {
    base::FilePath location = GetDataProvider()->GetOriginalPhotoLocation(
        components[1], components[3]);
    if (!location.empty()) {
      *local_file_path = location;
      return base::File::FILE_OK;
    }
  }

  return base::File::FILE_ERROR_NOT_FOUND;
}

IPhotoDataProvider* IPhotoFileUtil::GetDataProvider() {
  if (!imported_registry_)
    imported_registry_ = ImportedMediaGalleryRegistry::GetInstance();
  return imported_registry_->IPhotoDataProvider();
}

}  // namespace iphoto
