// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FILE_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FILE_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"

class ImportedMediaGalleryRegistry;

namespace itunes {

class ITunesDataProvider;

extern const char kITunesLibraryXML[];
extern const char kITunesMediaDir[];
extern const char kITunesMusicDir[];
extern const char kITunesAutoAddDir[];

class ITunesFileUtil : public NativeMediaFileUtil {
 public:
  explicit ITunesFileUtil(MediaPathFilter* media_path_filter);
  ~ITunesFileUtil() override;

 protected:
  // NativeMediaFileUtil overrides.
  void GetFileInfoOnTaskRunnerThread(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const GetFileInfoCallback& callback) override;
  void ReadDirectoryOnTaskRunnerThread(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const ReadDirectoryCallback& callback) override;
  void CreateSnapshotFileOnTaskRunnerThread(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const CreateSnapshotFileCallback& callback) override;
  base::File::Error GetFileInfoSync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      base::File::Info* file_info,
      base::FilePath* platform_path) override;
  base::File::Error ReadDirectorySync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      EntryList* file_list) override;
  base::File::Error DeleteDirectorySync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url) override;
  base::File::Error DeleteFileSync(storage::FileSystemOperationContext* context,
                                   const storage::FileSystemURL& url) override;
  base::File::Error CreateSnapshotFileSync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      base::File::Info* file_info,
      base::FilePath* platform_path,
      scoped_refptr<storage::ShareableFileReference>* file_ref) override;
  base::File::Error GetLocalFilePath(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      base::FilePath* local_file_path) override;

 private:
  void GetFileInfoWithFreshDataProvider(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const GetFileInfoCallback& callback,
      bool valid_parse);
  void ReadDirectoryWithFreshDataProvider(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const ReadDirectoryCallback& callback,
      bool valid_parse);
  virtual void CreateSnapshotFileWithFreshDataProvider(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const CreateSnapshotFileCallback& callback,
      bool valid_parse);

  virtual ITunesDataProvider* GetDataProvider();

  ImportedMediaGalleryRegistry* imported_registry_;

  base::WeakPtrFactory<ITunesFileUtil> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ITunesFileUtil);
};

}  // namespace itunes

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FILE_UTIL_H_
