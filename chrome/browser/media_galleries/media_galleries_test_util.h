// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_TEST_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_TEST_UTIL_H_

#include "base/basictypes.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/scoped_path_override.h"

#if defined(OS_WIN)
#include "base/test/test_reg_util_win.h"
#endif

namespace extensions {
class Extension;
}

namespace registry_util {
class RegistryOverrideManager;
}

class Profile;

#if defined(OS_MACOSX)
class MockPreferences;
#endif

scoped_refptr<extensions::Extension> AddMediaGalleriesApp(
    const std::string& name,
    const std::vector<std::string>& media_galleries_permissions,
    Profile* profile);

class EnsureMediaDirectoriesExists {
 public:
  EnsureMediaDirectoriesExists();
  ~EnsureMediaDirectoriesExists();

  int num_galleries() const { return num_galleries_; }

  base::FilePath GetFakeAppDataPath() const;
#if defined(OS_WIN)
  base::FilePath GetFakeLocalAppDataPath() const;
#endif
#if defined(OS_WIN) || defined(OS_MACOSX)
  void SetCustomPicasaAppDataPath(const base::FilePath& path);
  base::FilePath GetFakePicasaFoldersRootPath() const;
#endif

#if defined(OS_MACOSX)
  base::FilePath GetFakeITunesRootPath() const;
  base::FilePath GetFakeIPhotoRootPath() const;
#endif

 private:
  void Init();

  base::ScopedTempDir fake_dir_;

  int num_galleries_;

  scoped_ptr<base::ScopedPathOverride> app_data_override_;
  scoped_ptr<base::ScopedPathOverride> music_override_;
  scoped_ptr<base::ScopedPathOverride> pictures_override_;
  scoped_ptr<base::ScopedPathOverride> video_override_;
#if defined(OS_WIN)
  scoped_ptr<base::ScopedPathOverride> local_app_data_override_;

  registry_util::RegistryOverrideManager registry_override_;
#endif
#if defined(OS_MACOSX)
  scoped_ptr<MockPreferences> mac_preferences_;
#endif

  DISALLOW_COPY_AND_ASSIGN(EnsureMediaDirectoriesExists);
};

extern base::FilePath MakeMediaGalleriesTestingPath(const std::string& dir);

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_TEST_UTIL_H_
