// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_RESOURCE_GETTER_H_
#define MEDIA_BASE_ANDROID_MEDIA_RESOURCE_GETTER_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "media/base/media_export.h"

namespace media {

// Class for asynchronously retrieving resources for a media URL. All callbacks
// are executed on the caller's thread.
class MEDIA_EXPORT MediaResourceGetter {
 public:
  typedef base::Callback<void(const std::string&)> GetCookieCB;
  typedef base::Callback<void(const std::string&)> GetPlatformPathCB;
  typedef base::Callback<void(base::TimeDelta, int, int, bool)>
      ExtractMediaMetadataCB;
  virtual ~MediaResourceGetter();

  // Method for getting the cookies for a given URL.
  virtual void GetCookies(const GURL& url,
                          const GURL& first_party_for_cookies,
                          const GetCookieCB& callback) = 0;

  // Method for getting the platform path from a file system URL.
  virtual void GetPlatformPathFromFileSystemURL(
      const GURL& url,
      const GetPlatformPathCB& callback) = 0;

  // Extract the metadata from a media URL. Once completed, the provided
  // callback function will be run.
  virtual void ExtractMediaMetadata(
      const std::string& url,
      const std::string& cookies,
      const ExtractMediaMetadataCB& callback) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_RESOURCE_GETTER_H_
