// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_CACHE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_CACHE_OBSERVER_H_

#include <string>

namespace drive {

// Interface for classes that need to observe events from DriveCache.
// All events are notified on UI thread.
class DriveCacheObserver {
 public:
  // Triggered when a file has been pinned successfully.
  virtual void OnCachePinned(const std::string& resource_id,
                             const std::string& md5) {}

  // Triggered when a file has been unpinned successfully.
  virtual void OnCacheUnpinned(const std::string& resource_id,
                               const std::string& md5) {}

  // Triggered when a dirty file has been committed (saved) successfully.
  virtual void OnCacheCommitted(const std::string& resource_id) {}

 protected:
  virtual ~DriveCacheObserver() {}
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_CACHE_OBSERVER_H_
