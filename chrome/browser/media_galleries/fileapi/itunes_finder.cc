// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/itunes_finder.h"

#if defined(OS_WIN)
#include "chrome/browser/media_galleries/fileapi/itunes_finder_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/media_galleries/fileapi/itunes_finder_mac.h"
#endif

namespace itunes {

void FindITunesLibrary(const ITunesFinderCallback& callback) {
#if defined(OS_MACOSX)
  // Deletes itself on completion.
  new ITunesFinderMac(callback);
#elif defined(OS_WIN)
  // Deletes itself on completion.
  new ITunesFinderWin(callback);
#else
  callback.Run(std::string());
#endif
}

}  // namespace itunes
