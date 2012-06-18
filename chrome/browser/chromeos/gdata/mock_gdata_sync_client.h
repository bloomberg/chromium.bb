// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_MOCK_GDATA_SYNC_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_MOCK_GDATA_SYNC_CLIENT_H_
#pragma once

#include "chrome/browser/chromeos/gdata/gdata_sync_client.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
class Network;
class NetworkLibrary;
}

namespace gdata {
class GDataFileSystemInterface;

// Mock for GDataSyncClientInterface.
class MockGDataSyncClient : public GDataSyncClientInterface,
                            public GDataCache::Observer {
 public:
  MockGDataSyncClient();
  virtual ~MockGDataSyncClient();

  MOCK_METHOD0(Initialize, void());
  MOCK_METHOD0(OnCacheInitialized, void());
  MOCK_METHOD2(OnCachePinned, void(const std::string& resource_id,
                                   const std::string& md5));
  MOCK_METHOD2(OnCacheUnpinned, void(const std::string& resource_id,
                                     const std::string& md5));
  MOCK_METHOD1(OnCacheCommitted, void(const std::string& resource_id));
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_MOCK_GDATA_SYNC_CLIENT_H_
