// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_MAC_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_MAC_H_

#include "chrome/browser/media_galleries/fileapi/iapp_finder_mac.h"
#include "chrome/browser/media_galleries/fileapi/itunes_finder.h"

namespace itunes {

class ITunesFinderMac : public iapps::IAppFinderMac {
 public:
  explicit ITunesFinderMac(const ITunesFinderCallback& callback);
  virtual ~ITunesFinderMac();

 private:
  virtual base::FilePath ExtractPath(NSString* path_ns) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ITunesFinderMac);
};

}  // namespace itunes

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_MAC_H_
