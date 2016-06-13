// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/popular_sites.h"

#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"

base::FilePath ChromePopularSites::GetDirectory() {
  base::FilePath dir;
  PathService::Get(chrome::DIR_USER_DATA, &dir);
  return dir;  // empty if PathService::Get() failed.
}
