// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_FILE_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_FILE_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"

namespace picasa {

class PicasaDataProvider;

extern const char kPicasaDirAlbums[];
extern const char kPicasaDirFolders[];

// PicasaFileUtil virtual directory structure example:
//   - /albums/
//   - /albums/albumname 2013-08-21/
//   - /albums/albumname 2013-08-21/imagename.jpg
//   - /albums/duplicatename 2013-08-21/
//   - /albums/duplicatename 2013-08-21 (1)/
//   - /folders/
//   - /folders/My Pictures 2013-08-21/flower.jpg
//   - /folders/Photos 2013-08-21/
class PicasaFileUtil : public NativeMediaFileUtil {
 public:
  explicit PicasaFileUtil(MediaPathFilter* media_path_filter);
  virtual ~PicasaFileUtil();

 protected:
  // NativeMediaFileUtil overrides.
  virtual void GetFileInfoOnTaskRunnerThread(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const GetFileInfoCallback& callback) OVERRIDE;
  virtual void ReadDirectoryOnTaskRunnerThread(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const ReadDirectoryCallback& callback) OVERRIDE;
  virtual base::File::Error GetFileInfoSync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      base::File::Info* file_info,
      base::FilePath* platform_path) OVERRIDE;
  virtual base::File::Error ReadDirectorySync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      EntryList* file_list) OVERRIDE;
  virtual base::File::Error DeleteDirectorySync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url) OVERRIDE;
  virtual base::File::Error DeleteFileSync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url) OVERRIDE;
  virtual base::File::Error GetLocalFilePath(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      base::FilePath* local_file_path) OVERRIDE;

 private:
  void GetFileInfoWithFreshDataProvider(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const GetFileInfoCallback& callback,
      bool success);
  void ReadDirectoryWithFreshDataProvider(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const ReadDirectoryCallback& callback,
      bool success);

  virtual PicasaDataProvider* GetDataProvider();

  base::WeakPtrFactory<PicasaFileUtil> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PicasaFileUtil);
};

}  // namespace picasa

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_PICASA_FILE_UTIL_H_
