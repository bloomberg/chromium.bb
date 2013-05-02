// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_H_

#include <string>

#include "base/callback.h"

namespace itunes {

typedef base::Callback<void(const std::string&)> ITunesFinderCallback;

// ITunesFinder bounces to the File thread to find the iTunes library. If
// (and only if) the library exists, the callback is run on UI thread with a
// device id for the library. In either case, the class deletes itself.
class ITunesFinder {
 public:
  virtual ~ITunesFinder();

  // If the platform does not support iTunes or the iTunes library is not found,
  // |callback| will not be called. Otherwise, callback is run on the UI thread.
  static void FindITunesLibrary(ITunesFinderCallback callback);

 protected:
  explicit ITunesFinder(ITunesFinderCallback callback);

  // Per-OS implementation; calls PostResultToUIThread with a device id argument
  // or empty string when complete.
  virtual void FindITunesLibraryOnFileThread() = 0;

  void PostResultToUIThread(const std::string& result);

 private:
  void FinishOnUIThread();

  ITunesFinderCallback callback_;
  std::string result_;

  DISALLOW_COPY_AND_ASSIGN(ITunesFinder);
};

}  // namespace itunes

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_H_
