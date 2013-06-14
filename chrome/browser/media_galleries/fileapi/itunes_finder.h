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
// In either case, the class deletes itself.
class ITunesFinder {
 public:
  virtual ~ITunesFinder();

  // |callback| runs on the UI thread.
  static void FindITunesLibrary(const ITunesFinderCallback& callback);

 protected:
  explicit ITunesFinder(const ITunesFinderCallback& callback);

  // Called on the FILE thread. Bounces |unique_id| back to the UI thread.
  void PostResultToUIThread(const std::string& unique_id);

 private:
  // Per-OS implementation; always calls PostResultToUIThread() when complete,
  // with a unique id argument on success or empty string on failure.
  virtual void FindITunesLibraryOnFileThread() = 0;

  // Converts |unique_id| to a media device id and passes it to |callback_|.
  void FinishOnUIThread(const std::string& unique_id) const;

  // Only accessed on the UI thread.
  const ITunesFinderCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ITunesFinder);
};

}  // namespace itunes

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_H_
