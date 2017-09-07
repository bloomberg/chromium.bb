// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_IMPORTED_MEDIA_GALLERY_REGISTRY_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_IMPORTED_MEDIA_GALLERY_REGISTRY_H_

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "build/build_config.h"

namespace itunes {
class ITunesDataProvider;
class ITunesDataProviderTest;
}

// This class lives on the MediaTaskRunner thread. It has some static
// methods which are called on the UI thread.
//
// MediaTaskRunner is not guaranteed to be one thread, but it is guaranteed
// to be a series of sequential calls. See SequencedTaskRunner for details.
class ImportedMediaGalleryRegistry {
 public:
  static ImportedMediaGalleryRegistry* GetInstance();

  void Initialize();

  // Should be called on the UI thread only.
  bool RegisterITunesFilesystemOnUIThread(
      const std::string& fs_name,
      const base::FilePath& xml_library_path);

  bool RevokeImportedFilesystemOnUIThread(const std::string& fs_name);

  // Path where all virtual file systems are "mounted."
  base::FilePath ImportedRoot();

  // Should be called on the MediaTaskRunner thread only.
#if defined(OS_WIN) || defined(OS_MACOSX)
  static itunes::ITunesDataProvider* ITunesDataProvider();
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

 private:
  friend struct base::LazyInstanceTraitsBase<ImportedMediaGalleryRegistry>;
  friend class itunes::ITunesDataProviderTest;

  ImportedMediaGalleryRegistry();
  virtual ~ImportedMediaGalleryRegistry();

#if defined(OS_WIN) || defined(OS_MACOSX)
  void RegisterITunesFileSystem(const base::FilePath& xml_library_path);
  void RevokeITunesFileSystem();
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

  base::FilePath imported_root_;

#if defined(OS_WIN) || defined(OS_MACOSX)
  // The data providers are only set or accessed on the task runner thread.
  std::unique_ptr<itunes::ITunesDataProvider> itunes_data_provider_;

  // The remaining members are only accessed on the IO thread.
  std::set<std::string> itunes_fs_names_;

#ifndef NDEBUG
  base::FilePath itunes_xml_library_path_;
#endif
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

  DISALLOW_COPY_AND_ASSIGN(ImportedMediaGalleryRegistry);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_IMPORTED_MEDIA_GALLERY_REGISTRY_H_
