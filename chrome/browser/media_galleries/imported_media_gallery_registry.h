// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_IMPORTED_MEDIA_GALLERY_REGISTRY_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_IMPORTED_MEDIA_GALLERY_REGISTRY_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"

namespace picasa {
class PicasaDataProvider;
}

namespace chrome {

// This class lives on the MediaTaskRunner thread. It has some static
// methods which are called on the UI thread.
//
// MediaTaskRunner is not guaranteed to be one thread, but it is guaranteed
// to be a series of sequential calls. See SequencedTaskRunner for details.
class ImportedMediaGalleryRegistry {
 public:
  static ImportedMediaGalleryRegistry* GetInstance();

  // Should be called on the UI thread only.
  static std::string RegisterPicasaFilesystemOnUIThread(
      const base::FilePath& database_path);
  static bool RevokePicasaFilesystemOnUIThread(const std::string& fsid);

  // Should be called on the MediaTaskRunner thread only.
  static picasa::PicasaDataProvider* picasa_data_provider();

 private:
  friend struct base::DefaultLazyInstanceTraits<ImportedMediaGalleryRegistry>;

  ImportedMediaGalleryRegistry();
  virtual ~ImportedMediaGalleryRegistry();

  void RegisterPicasaFileSystem(const base::FilePath& database_path);
  void RevokePicasaFileSystem();

  scoped_ptr<picasa::PicasaDataProvider> picasa_data_provider_;

  int picasa_filesystems_count_;

  DISALLOW_COPY_AND_ASSIGN(ImportedMediaGalleryRegistry);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_IMPORTED_MEDIA_GALLERY_REGISTRY_H_
