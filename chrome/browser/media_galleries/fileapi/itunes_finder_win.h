// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_WIN_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_WIN_H_

#include "chrome/browser/media_galleries/fileapi/itunes_finder.h"

namespace itunes {

class ITunesFinderWin : public ITunesFinder {
 public:
  explicit ITunesFinderWin(ITunesFinderCallback callback);
  virtual ~ITunesFinderWin();

 protected:
  virtual void FindITunesLibraryOnFileThread() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ITunesFinderWin);
};

}  // namespace itunes

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_WIN_H_
