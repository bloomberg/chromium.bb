// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/itunes_finder_win.h"

#include "base/logging.h"

namespace itunes {

ITunesFinderWin::ITunesFinderWin(ITunesFinderCallback callback)
    : ITunesFinder(callback) {
}

ITunesFinderWin::~ITunesFinderWin() {}

void ITunesFinderWin::FindITunesLibraryOnFileThread() {
  PostResultToUIThread(std::string());
  NOTIMPLEMENTED();
}

}  // namespace itunes
