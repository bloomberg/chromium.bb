// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_H_

#include <string>

#include "base/callback.h"

namespace itunes {

typedef base::Callback<void(const std::string&)> ITunesFinderCallback;

// ITunesFinder bounces to the FILE thread to find the iTunes library. If
// (and only if) the library exists, the callback is run on UI thread with a
// device id for the library. In either case, the class deletes itself.
class ITunesFinder {
 public:
  virtual ~ITunesFinder();

  // If the platform does not support iTunes or the iTunes library is not found,
  // |callback| will not be called. Otherwise, callback is run on the UI thread.
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
