// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_POPULAR_SITES_H_
#define CHROME_BROWSER_ANDROID_NTP_POPULAR_SITES_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"

class Profile;

namespace ntp_tiles {
class PopularSites;
}  // namespace ntp_tiles

// TODO(sfiera): move to chrome_popular_sites.h
class ChromePopularSites {
 public:
  static std::unique_ptr<ntp_tiles::PopularSites> NewForProfile(
      Profile* profile);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ChromePopularSites);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_POPULAR_SITES_H_
