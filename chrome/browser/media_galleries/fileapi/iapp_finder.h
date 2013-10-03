// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IAPP_FINDER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IAPP_FINDER_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/storage_monitor/storage_info.h"

namespace iapps {

typedef base::Callback<void(const std::string&)> IAppFinderCallback;

// IAppFinder is a framework for finding application data for iTunes and
// iPhoto. It handles running the finding task on the file thread and
// constructing the device id out of the found location. If found, a device
// id is returned, if not, an empty string is returned through the callback.
class IAppFinder {
 public:
  virtual ~IAppFinder();

 protected:
  IAppFinder(StorageInfo::Type type, const IAppFinderCallback& callback);

  // Per-OS implementation; always calls PostResultToUIThread() when complete,
  // with a unique id argument on success or empty string on failure.
  virtual void FindIAppOnFileThread() = 0;

  // Called on the FILE thread. Bounces |unique_id| back to the UI thread.
  void PostResultToUIThread(const std::string& unique_id);

 private:
  // Converts |unique_id| to a media device id and passes it to |callback_|.
  void FinishOnUIThread(const std::string& unique_id) const;

  // Only accessed on the UI thread.
  const StorageInfo::Type type_;
  const IAppFinderCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(IAppFinder);
};

}  // namespace iapps

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_IAPP_FINDER_H_
