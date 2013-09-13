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

namespace extensions {
class Extension;
}

class Profile;

scoped_refptr<extensions::Extension> AddMediaGalleriesApp(
    const std::string& name,
    const std::vector<std::string>& media_galleries_permissions,
    Profile* profile);

class EnsureMediaDirectoriesExists {
 public:
  EnsureMediaDirectoriesExists();
  ~EnsureMediaDirectoriesExists();

  int num_galleries() const { return num_galleries_; }

 private:
  void Init();

  base::ScopedTempDir fake_dir_;

  int num_galleries_;

  scoped_ptr<base::ScopedPathOverride> appdir_override_;
  scoped_ptr<base::ScopedPathOverride> music_override_;
  scoped_ptr<base::ScopedPathOverride> pictures_override_;
  scoped_ptr<base::ScopedPathOverride> video_override_;

  DISALLOW_COPY_AND_ASSIGN(EnsureMediaDirectoriesExists);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_TEST_UTIL_H_
