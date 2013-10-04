// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IPHOTO_FINDER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IPHOTO_FINDER_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/media_galleries/fileapi/iapp_finder_mac.h"

namespace iphoto {

typedef base::Callback<void(const std::string&)> IPhotoFinderCallback;

// IPhotoFinder looks for the iPhoto library in an asynchronous manner and
// calls the given IPhotoFinderCallback on the UI thread as soon as it knows
// the result. If an iPhoto library exists, the IPhotoFinderCallback gets the
// device id for the library. If an iPhoto library does not exist, or the OS
// does not support iPhoto, then the callback result is an empty string.
// |callback| runs on the UI thread. This class deletes itself.
class IPhotoFinder : public iapps::IAppFinderMac {
 public:
  explicit IPhotoFinder(const IPhotoFinderCallback& callback);
  virtual ~IPhotoFinder();

 private:
  virtual base::FilePath ExtractPath(NSString* path_ns) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(IPhotoFinder);
};

}  // namespace iphoto

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IPHOTO_FINDER_H_
