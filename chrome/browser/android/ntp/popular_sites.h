// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_POPULAR_SITES_H_
#define CHROME_BROWSER_ANDROID_NTP_POPULAR_SITES_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/ntp_tiles/popular_sites.h"

// TODO(sfiera): move to chrome_popular_sites.h
class ChromePopularSites {
 public:
  static base::FilePath GetDirectory();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ChromePopularSites);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_POPULAR_SITES_H_
