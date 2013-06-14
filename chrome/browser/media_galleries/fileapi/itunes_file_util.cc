// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/itunes_file_util.h"

#include "base/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media_galleries/fileapi/itunes_data_provider.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_mount_point_provider.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/native_file_util.h"
#include "webkit/common/blob/shareable_file_reference.h"
#include "webkit/common/fileapi/file_system_util.h"

using fileapi::DirectoryEntry;

namespace itunes {

namespace {

const char kiTunesLibraryXML[] = "iTunes Music Library.xml";
const char kiTunesMediaDir[] = "iTunes Media";

base::PlatformFileError MakeDirectoryFileInfo(
    base::PlatformFileInfo* file_info) {
  base::PlatformFileInfo result;
  result.is_directory = true;
  *file_info = result;
  return base::PLATFORM_FILE_OK;
}

}  // namespace

ItunesFileUtil::ItunesFileUtil()
    : weak_factory_(this),
      imported_registry_(NULL)  {
}

ItunesFileUtil::~ItunesFileUtil() {
}

void ItunesFileUtil::GetFileInfoOnTaskRunnerThread(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  GetDataProvider()->RefreshData(
      base::Bind(&ItunesFileUtil::GetFileInfoWithFreshDataProvider,
                 weak_factory_.GetWeakPtr(), context, url, callback));
}

void ItunesFileUtil::ReadDirectoryOnTaskRunnerThread(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  GetDataProvider()->RefreshData(
      base::Bind(&ItunesFileUtil::ReadDirectoryWithFreshDataProvider,
                 weak_factory_.GetWeakPtr(), context, url, callback));
}

void ItunesFileUtil::CreateSnapshotFileOnTaskRunnerThread(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  GetDataProvider()->RefreshData(
      base::Bind(&ItunesFileUtil::CreateSnapshotFileWithFreshDataProvider,
                 weak_factory_.GetWeakPtr(), context, url, callback));
}

// Contents of the iTunes media gallery:
//   /                                          - root directory
//   /iTunes Music Library.xml                  - library xml file
//   /iTunes Media/<Artist>/<Album>/<Track>     - tracks
//
base::PlatformFileError ItunesFileUtil::GetFileInfoSync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    base::PlatformFileInfo* file_info,
    base::FilePath* platform_path) {
  std::vector<std::string> components;
  fileapi::VirtualPath::GetComponentsUTF8Unsafe(url.path(), &components);

  if (components.size() == 0)
    return MakeDirectoryFileInfo(file_info);

  if (components.size() == 1 && components[0] == kiTunesLibraryXML) {
    // We can't just call NativeMediaFileUtil::GetFileInfoSync() here because it
    // uses the MediaPathFilter. At this point, |library_path_| is known good
    // because GetFileInfoWithFreshDataProvider() gates access to this method.
    base::FilePath file_path = GetDataProvider()->library_path();
    if (platform_path)
      *platform_path = file_path;
    return fileapi::NativeFileUtil::GetFileInfo(file_path, file_info);
  }

  if (components[0] != kiTunesMediaDir || components.size() > 4)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  switch (components.size()) {
    case 1:
      return MakeDirectoryFileInfo(file_info);

    case 2:
      if (GetDataProvider()->KnownArtist(components[1]))
        return MakeDirectoryFileInfo(file_info);
      break;

    case 3:
      if (GetDataProvider()->KnownAlbum(components[1], components[2]))
        return MakeDirectoryFileInfo(file_info);
      break;

    case 4: {
      base::FilePath location =
          GetDataProvider()->GetTrackLocation(components[1], components[2],
                                              components[3]);
      if (!location.empty()) {
        return NativeMediaFileUtil::GetFileInfoSync(context, url, file_info,
                                                    platform_path);
      }
      break;
    }

    default:
      NOTREACHED();
  }

  return base::PLATFORM_FILE_ERROR_NOT_FOUND;
}

base::PlatformFileError ItunesFileUtil::ReadDirectorySync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    EntryList* file_list) {
  DCHECK(file_list->empty());
  std::vector<std::string> components;
  fileapi::VirtualPath::GetComponentsUTF8Unsafe(url.path(), &components);

  if (components.size() == 0) {
    base::PlatformFileInfo xml_info;
    if (!file_util::GetFileInfo(GetDataProvider()->library_path(), &xml_info))
      return base::PLATFORM_FILE_ERROR_IO;
    file_list->push_back(DirectoryEntry(kiTunesLibraryXML,
                                        DirectoryEntry::FILE,
                                        xml_info.size, xml_info.last_modified));
    file_list->push_back(DirectoryEntry(kiTunesMediaDir,
                                        DirectoryEntry::DIRECTORY,
                                        0, base::Time()));
    return base::PLATFORM_FILE_OK;
  }

  if (components.size() == 1 && components[0] == kiTunesLibraryXML)
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;

  if (components[0] != kiTunesMediaDir || components.size() > 4)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  if (components.size() == 1) {
    std::set<ITunesDataProvider::ArtistName> artists =
        GetDataProvider()->GetArtistNames();
    std::set<ITunesDataProvider::ArtistName>::const_iterator it;
    for (it = artists.begin(); it != artists.end(); ++it)
      file_list->push_back(DirectoryEntry(*it, DirectoryEntry::DIRECTORY,
                                          0, base::Time()));
    return base::PLATFORM_FILE_OK;
  }

  if (components.size() == 2) {
    std::set<ITunesDataProvider::AlbumName> albums =
        GetDataProvider()->GetAlbumNames(components[1]);
    if (albums.size() == 0)
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    std::set<ITunesDataProvider::AlbumName>::const_iterator it;
    for (it = albums.begin(); it != albums.end(); ++it)
      file_list->push_back(DirectoryEntry(*it, DirectoryEntry::DIRECTORY,
                                          0, base::Time()));
    return base::PLATFORM_FILE_OK;
  }

  if (components.size() == 3) {
    ITunesDataProvider::Album album =
        GetDataProvider()->GetAlbum(components[1], components[2]);
    if (album.size() == 0)
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    chrome::MediaPathFilter* path_filter =
        context->GetUserValue<chrome::MediaPathFilter*>(
            chrome::MediaFileSystemMountPointProvider::kMediaPathFilterKey);
    ITunesDataProvider::Album::const_iterator it;
    for (it = album.begin(); it != album.end(); ++it) {
      base::PlatformFileInfo file_info;
      if (path_filter->Match(it->second) &&
          file_util::GetFileInfo(it->second, &file_info)) {
        file_list->push_back(DirectoryEntry(it->first, DirectoryEntry::FILE,
                                            file_info.size,
                                            file_info.last_modified));
      }
    }
    return base::PLATFORM_FILE_OK;
  }

  // At this point, the only choice is one of two errors, but figuring out
  // which one is required.
  DCHECK_EQ(4UL, components.size());
  base::FilePath location;
  location = GetDataProvider()->GetTrackLocation(components[1], components[2],
                                                 components[3]);
  if (!location.empty())
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
  return base::PLATFORM_FILE_ERROR_NOT_FOUND;
}

base::PlatformFileError ItunesFileUtil::CreateSnapshotFileSync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    base::PlatformFileInfo* file_info,
    base::FilePath* platform_path,
    scoped_refptr<webkit_blob::ShareableFileReference>* file_ref) {
  DCHECK(!url.path().IsAbsolute());
  if (url.path() != base::FilePath().AppendASCII(kiTunesLibraryXML)) {
    return NativeMediaFileUtil::CreateSnapshotFileSync(context, url, file_info,
                                                       platform_path, file_ref);
  }

  // The following code is different than
  // NativeMediaFileUtil::CreateSnapshotFileSync in that it knows that the
  // library xml file is not a directory and it doesn't run mime sniffing on the
  // file. The only way to get here is by way of
  // CreateSnapshotFileWithFreshDataProvider() so the file has already been
  // parsed and deemed valid.
  *file_ref = scoped_refptr<webkit_blob::ShareableFileReference>();
  return GetFileInfoSync(context, url, file_info, platform_path);
}

base::PlatformFileError ItunesFileUtil::GetLocalFilePath(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    base::FilePath* local_file_path) {
  // Should only get here for files, i.e. the xml file and tracks.
  std::vector<std::string> components;
  fileapi::VirtualPath::GetComponentsUTF8Unsafe(url.path(), &components);

  if (components.size() == 1 && components[0] == kiTunesLibraryXML) {
    *local_file_path = GetDataProvider()->library_path();
    return base::PLATFORM_FILE_OK;
  }

  if (components[0] != kiTunesMediaDir || components.size() != 4)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  *local_file_path = GetDataProvider()->GetTrackLocation(components[1],
                                                         components[2],
                                                         components[3]);
  if (!local_file_path->empty())
    return base::PLATFORM_FILE_OK;

  return base::PLATFORM_FILE_ERROR_NOT_FOUND;
}

void ItunesFileUtil::GetFileInfoWithFreshDataProvider(
    fileapi::FileSystemOperationContext* context,
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
  NativeMediaFileUtil::GetFileInfoOnTaskRunnerThread(context, url, callback);
}

void ItunesFileUtil::ReadDirectoryWithFreshDataProvider(
    fileapi::FileSystemOperationContext* context,
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
  NativeMediaFileUtil::ReadDirectoryOnTaskRunnerThread(context, url, callback);
}

void ItunesFileUtil::CreateSnapshotFileWithFreshDataProvider(
    fileapi::FileSystemOperationContext* context,
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
  NativeMediaFileUtil::CreateSnapshotFileOnTaskRunnerThread(context, url,
                                                            callback);
}

ITunesDataProvider* ItunesFileUtil::GetDataProvider() {
  if (!imported_registry_)
    imported_registry_ = chrome::ImportedMediaGalleryRegistry::GetInstance();
  return imported_registry_->ITunesDataProvider();
}

}  // namespace itunes
