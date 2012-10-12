// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_MOCK_DRIVE_CACHE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_MOCK_DRIVE_CACHE_OBSERVER_H_

#include <string>

#include "chrome/browser/chromeos/drive/drive_cache_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace drive {

// Mock for DriveCache::Observer.
class MockDriveCacheObserver : public DriveCacheObserver {
 public:
  MockDriveCacheObserver();
  virtual ~MockDriveCacheObserver();

  MOCK_METHOD2(OnCachePinned, void(const std::string& resource_id,
                                   const std::string& md5));
  MOCK_METHOD2(OnCacheUnpinned, void(const std::string& resource_id,
                                     const std::string& md5));
  MOCK_METHOD1(OnCacheCommitted, void(const std::string& resource_id));
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_MOCK_DRIVE_CACHE_OBSERVER_H_
