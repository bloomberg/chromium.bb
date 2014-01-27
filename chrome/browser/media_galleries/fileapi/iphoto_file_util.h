// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IPHOTO_FILE_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IPHOTO_FILE_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"

class ImportedMediaGalleryRegistry;

namespace iphoto {

class IPhotoDataProvider;

// Presents a virtual file system containing iPhoto contents in the
// following organization:
// /                    = virtual root
// |- /Albums
//    |- /AlbumName     = An album entry by name.
//       |- pic1.jpg    = Entries for photos in the album.
//       |- pic2.jpg
//    |- /AnotherAlbum
//       |- pic3.jpg
//       |- /originals  = A directory for originals.
//          |- pic3.jpg = Original picture. Same name as album photo.

extern const char kIPhotoAlbumsDir[];

class IPhotoFileUtil : public NativeMediaFileUtil {
 public:
  explicit IPhotoFileUtil(MediaPathFilter* media_path_filter);
  virtual ~IPhotoFileUtil();

 protected:
  // NativeMediaFileUtil overrides.
  virtual void GetFileInfoOnTaskRunnerThread(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const GetFileInfoCallback& callback) OVERRIDE;
  virtual void ReadDirectoryOnTaskRunnerThread(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const ReadDirectoryCallback& callback) OVERRIDE;
  virtual void CreateSnapshotFileOnTaskRunnerThread(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const CreateSnapshotFileCallback& callback) OVERRIDE;
  virtual base::File::Error GetFileInfoSync(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url,
      base::File::Info* file_info,
      base::FilePath* platform_path) OVERRIDE;
  virtual base::File::Error ReadDirectorySync(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url,
      EntryList* file_list) OVERRIDE;
  virtual base::File::Error DeleteDirectorySync(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url) OVERRIDE;
  virtual base::File::Error DeleteFileSync(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url) OVERRIDE;
  virtual base::File::Error GetLocalFilePath(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url,
      base::FilePath* local_file_path) OVERRIDE;

 private:
  void GetFileInfoWithFreshDataProvider(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const GetFileInfoCallback& callback,
      bool valid_parse);
  void ReadDirectoryWithFreshDataProvider(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const ReadDirectoryCallback& callback,
      bool valid_parse);
  virtual void CreateSnapshotFileWithFreshDataProvider(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const CreateSnapshotFileCallback& callback,
      bool valid_parse);

  virtual IPhotoDataProvider* GetDataProvider();

  base::WeakPtrFactory<IPhotoFileUtil> weak_factory_;

  ImportedMediaGalleryRegistry* imported_registry_;

  DISALLOW_COPY_AND_ASSIGN(IPhotoFileUtil);
};

}  // namespace iphoto

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IPHOTO_FILE_UTIL_H_
