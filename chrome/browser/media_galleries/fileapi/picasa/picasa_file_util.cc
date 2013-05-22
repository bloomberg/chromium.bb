// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/picasa/picasa_file_util.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media_galleries/fileapi/filtering_file_enumerator.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_mount_point_provider.h"
#include "chrome/browser/media_galleries/fileapi/picasa/picasa_album_table_reader.h"
#include "chrome/browser/media_galleries/fileapi/picasa/picasa_data_provider.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"

using base::FilePath;
using fileapi::FileSystemOperationContext;
using fileapi::FileSystemURL;

namespace picasa {

namespace {

class DirectorySkippingFileEnumerator
    : public fileapi::FileSystemFileUtil::AbstractFileEnumerator {
 public:
  DirectorySkippingFileEnumerator(
      scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator>
          base_enumerator)
      : base_enumerator_(base_enumerator.Pass()) {
  }
  virtual ~DirectorySkippingFileEnumerator() {}

  virtual base::FilePath Next() OVERRIDE {
    while (true) {
      base::FilePath next = base_enumerator_->Next();
      if (next.empty() || !IsDirectory())
        return next;
    }
  }

  virtual int64 Size() OVERRIDE {
    return base_enumerator_->Size();
  }

  virtual base::Time LastModifiedTime() OVERRIDE {
    return base_enumerator_->LastModifiedTime();
  }

  virtual bool IsDirectory() OVERRIDE {
    DCHECK(!base_enumerator_->IsDirectory());
    return base_enumerator_->IsDirectory();
  }

 private:
  // The file enumerator to be wrapped.
  scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator>
      base_enumerator_;
};

struct VirtualFile {
  VirtualFile(const base::FilePath& path, int64 size,
              const base::Time& last_modified_time, bool is_directory)
      : path(path),
        size(size),
        last_modified_time(last_modified_time),
        is_directory(is_directory) {}

  base::FilePath path;
  int64 size;
  base::Time last_modified_time;
  bool is_directory;
};

class VirtualFileEnumerator
    : public fileapi::FileSystemFileUtil::AbstractFileEnumerator {
 public:
  explicit VirtualFileEnumerator(const std::vector<VirtualFile>& files)
      : files_(files), next_called_(false), index_(0) {}

  virtual base::FilePath Next() OVERRIDE {
    if (next_called_ && index_ < files_.size())
      ++index_;

    next_called_ = true;

    if (index_ >= files_.size())
      return FilePath();
    return files_[index_].path;
  }

  virtual int64 Size() OVERRIDE {
    if (index_ >= files_.size())
      return 0;
    return files_[index_].size;
  }

  virtual base::Time LastModifiedTime() OVERRIDE {
    if (index_ >= files_.size())
      return base::Time();
    return files_[index_].last_modified_time;
  }

  virtual bool IsDirectory() OVERRIDE {
    if (index_ >= files_.size())
      return false;
    return files_[index_].is_directory;
  }

 private:
  std::vector<VirtualFile> files_;
  bool next_called_;
  size_t index_;
};

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

// Get UTF-8 components of FilePath to match against Picasa's UTF-8 strings.
std::vector<std::string> GetComponents(
    const fileapi::FileSystemURL& url) {
  std::vector<base::FilePath::StringType> stringtype_components;

  url.path().GetComponents(&stringtype_components);

  std::vector<std::string> utf8_components;

  for (std::vector<base::FilePath::StringType>::const_iterator it =
           stringtype_components.begin();
       it != stringtype_components.end(); ++it) {
    // This is ugly and unsafe, but the same as FilePath::AsUTF8Unsafe().
#if defined(OS_WIN)
    utf8_components.push_back(base::WideToUTF8(*it));
#elif defined(OS_MACOSX)
    utf8_components.push_back(*it);
#else
    // TODO(tommycli): Remove whole compilation unit from all but Win/Mac.
    // NOTIMPLEMENTED();
    utf8_components.push_back(base::WideToUTF8(base::SysNativeMBToWide(*it)));
#endif
  }

  return utf8_components;
}


}  // namespace

const char kPicasaDirAlbums[]  = "albums";
const char kPicasaDirFolders[] = "folders";

PicasaFileUtil::PicasaFileUtil() {}

PicasaFileUtil::~PicasaFileUtil() {}

base::PlatformFileError PicasaFileUtil::GetFileInfo(
    FileSystemOperationContext* context, const FileSystemURL& url,
    base::PlatformFileInfo* file_info, base::FilePath* platform_path) {
  DCHECK(context);
  DCHECK(file_info);
  DCHECK(platform_path);

  *platform_path = base::FilePath();

  std::vector<std::string> path_components = GetComponents(url);

  switch (path_components.size()) {
    case 0:
      // Root directory.
      file_info->is_directory = true;
      return base::PLATFORM_FILE_OK;
    case 1:
      if (path_components[0] == kPicasaDirAlbums ||
          path_components[0] == kPicasaDirFolders) {
        file_info->is_directory = true;
        return base::PLATFORM_FILE_OK;
      }

      break;
    case 2:
      if (path_components[0] == kPicasaDirAlbums) {
        scoped_ptr<AlbumMap> album_map = DataProvider()->GetAlbums();
        base::PlatformFileError error =
            FindAlbumInfo(path_components[1], album_map.get(), NULL);
        if (error != base::PLATFORM_FILE_OK)
          return error;

        file_info->is_directory = true;
        return base::PLATFORM_FILE_OK;
      }

      if (path_components[0] == kPicasaDirFolders) {
        return NativeMediaFileUtil::GetFileInfo(context, url, file_info,
                                                platform_path);
      }
      break;
    case 3:
      // NativeMediaFileUtil::GetInfo calls into virtual function
      // PicasaFileUtil::GetLocalFilePath, and that will handle both
      // album contents and folder contents.
      base::PlatformFileError result = NativeMediaFileUtil::GetFileInfo(
          context, url, file_info, platform_path);

      DCHECK(path_components[0] == kPicasaDirAlbums ||
             path_components[0] == kPicasaDirFolders ||
             result == base::PLATFORM_FILE_ERROR_NOT_FOUND);

      return result;
  }

  return base::PLATFORM_FILE_ERROR_NOT_FOUND;
}

scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator>
PicasaFileUtil::CreateFileEnumerator(FileSystemOperationContext* context,
                                     const FileSystemURL& url) {
  DCHECK(context);
  std::vector<std::string> path_components = GetComponents(url);

  std::vector<VirtualFile> files;
  FilePath folders_path = base::FilePath().AppendASCII(kPicasaDirFolders);

  switch (path_components.size()) {
    case 0: {
      // Root directory.
      FilePath albums_path = base::FilePath().AppendASCII(kPicasaDirAlbums);
      files.push_back(VirtualFile(albums_path, 0, base::Time(), true));
      files.push_back(VirtualFile(folders_path, 0, base::Time(), true));
      break;
    }
    case 1:
      if (path_components[0] == kPicasaDirAlbums) {
        scoped_ptr<AlbumMap> albums = DataProvider()->GetAlbums();
        if (!albums)
          return scoped_ptr<AbstractFileEnumerator>(new EmptyFileEnumerator());

        // TODO(tommycli): Use AlbumMap enumerator to avoid copies.
        for (AlbumMap::const_iterator it = albums->begin();
             it != albums->end(); ++it) {
          files.push_back(VirtualFile(
              folders_path.Append(FilePath::FromUTF8Unsafe(it->first)),
              0,
              it->second.timestamp,
              true));
        }
      } else if (path_components[0] == kPicasaDirFolders) {
        scoped_ptr<AlbumMap> folders = DataProvider()->GetFolders();
        if (!folders)
          return scoped_ptr<AbstractFileEnumerator>(new EmptyFileEnumerator());

        // TODO(tommycli): Use AlbumMap enumerator to avoid copies.
        for (AlbumMap::const_iterator it = folders->begin();
             it != folders->end(); ++it) {
          files.push_back(VirtualFile(
              folders_path.Append(FilePath::FromUTF8Unsafe(it->first)),
              0,
              it->second.timestamp,
              true));
        }
      }
      break;
    case 2:
      if (path_components[0] == kPicasaDirAlbums) {
        // TODO(tommycli): Implement album contents.
      }

      if (path_components[0] == kPicasaDirFolders)
        return scoped_ptr<AbstractFileEnumerator>(
            new DirectorySkippingFileEnumerator(
                NativeMediaFileUtil::CreateFileEnumerator(context, url)));
      break;
  }

  return scoped_ptr<AbstractFileEnumerator>(new VirtualFileEnumerator(files));
}

base::PlatformFileError PicasaFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context, const FileSystemURL& url,
    base::FilePath* local_file_path) {
  DCHECK(local_file_path);
  DCHECK(url.is_valid());
  std::vector<std::string> path_components = GetComponents(url);

  switch (path_components.size()) {
    case 2:
      if (path_components[0] == kPicasaDirFolders) {
        scoped_ptr<AlbumMap> album_map = DataProvider()->GetFolders();
        AlbumInfo album_info;
        base::PlatformFileError error =
            FindAlbumInfo(path_components[1], album_map.get(), &album_info);
        if (error != base::PLATFORM_FILE_OK)
          return error;

        *local_file_path = album_info.path;
        return base::PLATFORM_FILE_OK;
      }
      break;
    case 3:
      if (path_components[0] == kPicasaDirAlbums) {
        scoped_ptr<AlbumMap> album_map = DataProvider()->GetAlbums();
        base::PlatformFileError error =
            FindAlbumInfo(path_components[1], album_map.get(), NULL);
        if (error != base::PLATFORM_FILE_OK)
          return error;

        // TODO(tommycli): Implement album contents.
        return base::PLATFORM_FILE_ERROR_NOT_FOUND;
      }

      if (path_components[0] == kPicasaDirFolders) {
        scoped_ptr<AlbumMap> album_map = DataProvider()->GetFolders();
        AlbumInfo album_info;
        base::PlatformFileError error =
            FindAlbumInfo(path_components[1], album_map.get(), &album_info);
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
  return chrome::ImportedMediaGalleryRegistry::picasa_data_provider();
}

}  // namespace picasa
