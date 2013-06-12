// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/picasa/picasa_file_util.h"

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_mount_point_provider.h"
#include "chrome/browser/media_galleries/fileapi/picasa/picasa_album_table_reader.h"
#include "chrome/browser/media_galleries/fileapi/picasa/picasa_data_provider.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/fileapi/file_system_util.h"

using base::FilePath;
using fileapi::FileSystemOperationContext;
using fileapi::FileSystemURL;

namespace picasa {

namespace {

fileapi::DirectoryEntry MakeDirectoryEntry(
    const base::FilePath& path,
    int64 size,
    const base::Time& last_modified_time,
    bool is_directory) {
  fileapi::DirectoryEntry entry;
  entry.name = path.BaseName().value();
  entry.is_directory = is_directory;
  entry.size = size;
  entry.last_modified_time = last_modified_time;
  return entry;
}

// |error| is only set when the method fails and the return is NULL.
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

}  // namespace

const char kPicasaDirAlbums[]  = "albums";
const char kPicasaDirFolders[] = "folders";

PicasaFileUtil::PicasaFileUtil() {}

PicasaFileUtil::~PicasaFileUtil() {}

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
        scoped_ptr<AlbumMap> album_map = DataProvider()->GetAlbums();
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

  FilePath folders_path = base::FilePath().AppendASCII(kPicasaDirFolders);

  switch (components.size()) {
    case 0: {
      // Root directory.
      FilePath albums_path = base::FilePath().AppendASCII(kPicasaDirAlbums);
      file_list->push_back(
          MakeDirectoryEntry(albums_path, 0, base::Time(), true));
      file_list->push_back(
          MakeDirectoryEntry(folders_path, 0, base::Time(), true));
      break;
    }
    case 1:
      if (components[0] == kPicasaDirAlbums) {
        scoped_ptr<AlbumMap> albums = DataProvider()->GetAlbums();
        if (!albums)
          return base::PLATFORM_FILE_ERROR_NOT_FOUND;

        for (AlbumMap::const_iterator it = albums->begin();
             it != albums->end(); ++it) {
          file_list->push_back(MakeDirectoryEntry(
              folders_path.Append(FilePath::FromUTF8Unsafe(it->first)),
              0,
              it->second.timestamp,
              true));
        }
      } else if (components[0] == kPicasaDirFolders) {
        scoped_ptr<AlbumMap> folders = DataProvider()->GetFolders();
        if (!folders)
          return base::PLATFORM_FILE_ERROR_NOT_FOUND;

        for (AlbumMap::const_iterator it = folders->begin();
             it != folders->end(); ++it) {
          file_list->push_back(MakeDirectoryEntry(
              folders_path.Append(FilePath::FromUTF8Unsafe(it->first)),
              0,
              it->second.timestamp,
              true));
        }
      }
      break;
    case 2:
      if (components[0] == kPicasaDirAlbums) {
        // TODO(tommycli): Implement album contents.
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
        scoped_ptr<AlbumMap> album_map = DataProvider()->GetFolders();
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
        scoped_ptr<AlbumMap> album_map = DataProvider()->GetAlbums();
        base::PlatformFileError error =
            FindAlbumInfo(components[1], album_map.get(), NULL);
        if (error != base::PLATFORM_FILE_OK)
          return error;

        // TODO(tommycli): Implement album contents.
        return base::PLATFORM_FILE_ERROR_NOT_FOUND;
      }

      if (components[0] == kPicasaDirFolders) {
        scoped_ptr<AlbumMap> album_map = DataProvider()->GetFolders();
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

PicasaDataProvider* PicasaFileUtil::DataProvider() {
  return chrome::ImportedMediaGalleryRegistry::PicasaDataProvider();
}

}  // namespace picasa
