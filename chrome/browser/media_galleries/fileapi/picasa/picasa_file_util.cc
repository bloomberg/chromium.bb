// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/picasa/picasa_file_util.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/media_galleries/fileapi/picasa/picasa_data_provider.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "chrome/common/media_galleries/picasa_types.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/native_file_util.h"
#include "webkit/common/fileapi/file_system_util.h"

using base::FilePath;
using fileapi::DirectoryEntry;
using fileapi::FileSystemOperationContext;
using fileapi::FileSystemURL;

namespace picasa {

namespace {

base::PlatformFileError FindAlbumInfo(const std::string& key,
                                      const AlbumMap* map,
                                      AlbumInfo* album_info) {
  if (!map)
    return base::PLATFORM_FILE_ERROR_FAILED;

  AlbumMap::const_iterator it = map->find(key);

  if (it == map->end())
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  if (album_info != NULL)
    *album_info = it->second;

  return base::PLATFORM_FILE_OK;
}

PicasaDataProvider::DataType GetDataTypeForURL(
    const fileapi::FileSystemURL& url) {
  std::vector<std::string> components;
  fileapi::VirtualPath::GetComponentsUTF8Unsafe(url.path(), &components);

  if (components.size() >= 2 && components[0] == kPicasaDirAlbums)
    return PicasaDataProvider::ALBUMS_IMAGES_DATA;

  return PicasaDataProvider::LIST_OF_ALBUMS_AND_FOLDERS_DATA;
}

}  // namespace

const char kPicasaDirAlbums[]  = "albums";
const char kPicasaDirFolders[] = "folders";

PicasaFileUtil::PicasaFileUtil(chrome::MediaPathFilter* media_path_filter)
    : chrome::NativeMediaFileUtil(media_path_filter),
      weak_factory_(this) {
}

PicasaFileUtil::~PicasaFileUtil() {}

void PicasaFileUtil::GetFileInfoOnTaskRunnerThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  GetDataProvider()->RefreshData(
      GetDataTypeForURL(url),
      base::Bind(&PicasaFileUtil::GetFileInfoWithFreshDataProvider,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&context),
                 url,
                 callback));
}

void PicasaFileUtil::ReadDirectoryOnTaskRunnerThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  GetDataProvider()->RefreshData(
      GetDataTypeForURL(url),
      base::Bind(&PicasaFileUtil::ReadDirectoryWithFreshDataProvider,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&context),
                 url,
                 callback));
}

base::PlatformFileError PicasaFileUtil::GetFileInfoSync(
    FileSystemOperationContext* context, const FileSystemURL& url,
    base::PlatformFileInfo* file_info, base::FilePath* platform_path) {
  DCHECK(context);
  DCHECK(file_info);
  DCHECK(platform_path);

  *platform_path = base::FilePath();

  std::vector<std::string> components;
  fileapi::VirtualPath::GetComponentsUTF8Unsafe(url.path(), &components);

  switch (components.size()) {
    case 0:
      // Root directory.
      file_info->is_directory = true;
      return base::PLATFORM_FILE_OK;
    case 1:
      if (components[0] == kPicasaDirAlbums ||
          components[0] == kPicasaDirFolders) {
        file_info->is_directory = true;
        return base::PLATFORM_FILE_OK;
      }

      break;
    case 2:
      if (components[0] == kPicasaDirAlbums) {
        scoped_ptr<AlbumMap> album_map = GetDataProvider()->GetAlbums();
        base::PlatformFileError error =
            FindAlbumInfo(components[1], album_map.get(), NULL);
        if (error != base::PLATFORM_FILE_OK)
          return error;

        file_info->is_directory = true;
        return base::PLATFORM_FILE_OK;
      }

      if (components[0] == kPicasaDirFolders) {
        return NativeMediaFileUtil::GetFileInfoSync(context, url, file_info,
                                                    platform_path);
      }
      break;
    case 3:
      // NativeMediaFileUtil::GetInfo calls into virtual function
      // PicasaFileUtil::GetLocalFilePath, and that will handle both
      // album contents and folder contents.
      base::PlatformFileError result = NativeMediaFileUtil::GetFileInfoSync(
          context, url, file_info, platform_path);

      DCHECK(components[0] == kPicasaDirAlbums ||
             components[0] == kPicasaDirFolders ||
             result == base::PLATFORM_FILE_ERROR_NOT_FOUND);

      return result;
  }

  return base::PLATFORM_FILE_ERROR_NOT_FOUND;
}

base::PlatformFileError PicasaFileUtil::ReadDirectorySync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    EntryList* file_list) {
  DCHECK(context);
  DCHECK(file_list);
  DCHECK(file_list->empty());

  base::PlatformFileInfo file_info;
  base::FilePath platform_directory_path;
  base::PlatformFileError error = GetFileInfoSync(
      context, url, &file_info, &platform_directory_path);

  if (error != base::PLATFORM_FILE_OK)
    return error;

  if (!file_info.is_directory)
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;

  std::vector<std::string> components;
  fileapi::VirtualPath::GetComponentsUTF8Unsafe(url.path(), &components);

  switch (components.size()) {
    case 0: {
      // Root directory.
      file_list->push_back(
          DirectoryEntry(kPicasaDirAlbums, DirectoryEntry::DIRECTORY, 0,
                         base::Time()));
      file_list->push_back(
          DirectoryEntry(kPicasaDirFolders, DirectoryEntry::DIRECTORY, 0,
                         base::Time()));
      break;
    }
    case 1:
      if (components[0] == kPicasaDirAlbums) {
        scoped_ptr<AlbumMap> albums = GetDataProvider()->GetAlbums();
        if (!albums)
          return base::PLATFORM_FILE_ERROR_NOT_FOUND;

        for (AlbumMap::const_iterator it = albums->begin();
             it != albums->end(); ++it) {
          file_list->push_back(
              DirectoryEntry(it->first, DirectoryEntry::DIRECTORY, 0,
                             it->second.timestamp));
        }
      } else if (components[0] == kPicasaDirFolders) {
        scoped_ptr<AlbumMap> folders = GetDataProvider()->GetFolders();
        if (!folders)
          return base::PLATFORM_FILE_ERROR_NOT_FOUND;

        for (AlbumMap::const_iterator it = folders->begin();
             it != folders->end(); ++it) {
          file_list->push_back(
              DirectoryEntry(it->first, DirectoryEntry::DIRECTORY, 0,
                             it->second.timestamp));
        }
      }
      break;
    case 2:
      if (components[0] == kPicasaDirAlbums) {
        scoped_ptr<AlbumMap> album_map = GetDataProvider()->GetAlbums();
        AlbumInfo album_info;
        base::PlatformFileError error =
            FindAlbumInfo(components[1], album_map.get(), &album_info);
        if (error != base::PLATFORM_FILE_OK)
          return error;

        scoped_ptr<AlbumImages> album_images =
            GetDataProvider()->FindAlbumImages(album_info.uid, &error);
        if (error != base::PLATFORM_FILE_OK)
          return error;

        for (AlbumImages::const_iterator it = album_images->begin();
             it != album_images->end();
             ++it) {
          fileapi::DirectoryEntry entry;
          base::PlatformFileInfo info;

          // Simply skip files that we can't get info on.
          if (fileapi::NativeFileUtil::GetFileInfo(it->second, &info) !=
              base::PLATFORM_FILE_OK) {
            continue;
          }

          file_list->push_back(DirectoryEntry(
              it->first, DirectoryEntry::FILE, info.size, info.last_modified));
        }
      }

      if (components[0] == kPicasaDirFolders) {
        EntryList super_list;
        base::PlatformFileError error =
            NativeMediaFileUtil::ReadDirectorySync(context, url, &super_list);
        if (error != base::PLATFORM_FILE_OK)
          return error;

        for (EntryList::const_iterator it = super_list.begin();
             it != super_list.end(); ++it) {
          if (!it->is_directory)
            file_list->push_back(*it);
        }
      }

      break;
  }

  return base::PLATFORM_FILE_OK;
}

base::PlatformFileError PicasaFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context, const FileSystemURL& url,
    base::FilePath* local_file_path) {
  DCHECK(local_file_path);
  DCHECK(url.is_valid());
  std::vector<std::string> components;
  fileapi::VirtualPath::GetComponentsUTF8Unsafe(url.path(), &components);

  switch (components.size()) {
    case 2:
      if (components[0] == kPicasaDirFolders) {
        scoped_ptr<AlbumMap> album_map = GetDataProvider()->GetFolders();
        AlbumInfo album_info;
        base::PlatformFileError error =
            FindAlbumInfo(components[1], album_map.get(), &album_info);
        if (error != base::PLATFORM_FILE_OK)
          return error;

        *local_file_path = album_info.path;
        return base::PLATFORM_FILE_OK;
      }
      break;
    case 3:
      if (components[0] == kPicasaDirAlbums) {
        scoped_ptr<AlbumMap> album_map = GetDataProvider()->GetAlbums();
        AlbumInfo album_info;
        base::PlatformFileError error =
            FindAlbumInfo(components[1], album_map.get(), &album_info);
        if (error != base::PLATFORM_FILE_OK)
          return error;

        scoped_ptr<AlbumImages> album_images =
            GetDataProvider()->FindAlbumImages(album_info.uid, &error);
        if (error != base::PLATFORM_FILE_OK)
          return error;

        AlbumImages::const_iterator it = album_images->find(components[2]);
        if (it == album_images->end())
          return base::PLATFORM_FILE_ERROR_NOT_FOUND;

        *local_file_path = it->second;
        return base::PLATFORM_FILE_OK;
      }

      if (components[0] == kPicasaDirFolders) {
        scoped_ptr<AlbumMap> album_map = GetDataProvider()->GetFolders();
        AlbumInfo album_info;
        base::PlatformFileError error =
            FindAlbumInfo(components[1], album_map.get(), &album_info);
        if (error != base::PLATFORM_FILE_OK)
          return error;

        // Not part of this class's mandate to check that it actually exists.
        *local_file_path = album_info.path.Append(url.path().BaseName());
        return base::PLATFORM_FILE_OK;
      }

      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
      break;
  }

  // All other cases don't have a local path. The valid cases should be
  // intercepted by GetFileInfo()/CreateFileEnumerator(). Invalid cases
  // return a NOT_FOUND error.
  return base::PLATFORM_FILE_ERROR_NOT_FOUND;
}

void PicasaFileUtil::GetFileInfoWithFreshDataProvider(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const GetFileInfoCallback& callback,
    bool success) {
  if (!success) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(
            callback, base::PLATFORM_FILE_ERROR_IO, base::PlatformFileInfo()));
    return;
  }
  NativeMediaFileUtil::GetFileInfoOnTaskRunnerThread(
      context.Pass(), url, callback);
}

void PicasaFileUtil::ReadDirectoryWithFreshDataProvider(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const ReadDirectoryCallback& callback,
    bool success) {
  if (!success) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(callback, base::PLATFORM_FILE_ERROR_IO, EntryList(), false));
    return;
  }
  NativeMediaFileUtil::ReadDirectoryOnTaskRunnerThread(
      context.Pass(), url, callback);
}

PicasaDataProvider* PicasaFileUtil::GetDataProvider() {
  return chrome::ImportedMediaGalleryRegistry::PicasaDataProvider();
}

}  // namespace picasa
