// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_H_

#include <string>

#include "base/callback.h"

namespace itunes {

typedef base::Callback<void(const std::string&)> ITunesFinderCallback;

// ITunesFinder looks for the iTunes library in an asynchronous manner and
// calls the given ITunesFinderCallback on the UI thread as soon as it knows
// the result. If an iTunes library exists, the ITunesFinderCallback gets the
// device id for the library. If an iTunes library does not exist, or the OS
// does not support iTunes, then the callback result is an empty string.
void FindITunesLibrary(const ITunesFinderCallback& callback);

}  // namespace itunes

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_H_
