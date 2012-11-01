// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_COOKIE_GETTER_H_
#define MEDIA_BASE_ANDROID_COOKIE_GETTER_H_

#include <string>

#include "base/callback.h"
#include "media/base/media_export.h"

namespace media {

// Class for asynchronously retrieving the cookies for a given URL.
class MEDIA_EXPORT CookieGetter {
 public:
  typedef base::Callback<void(const std::string&)> GetCookieCB;
  virtual ~CookieGetter();

  // Method for getting the cookies for a given URL.
  // The callback is executed on the caller's thread.
  virtual void GetCookies(const std::string& url,
                          const std::string& first_party_for_cookies,
                          const GetCookieCB& callback) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_COOKIE_GETTER_H_
