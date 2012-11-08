// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_REMOTE_FILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_REMOTE_FILE_SYNC_SERVICE_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/fileapi/syncable/sync_callbacks.h"

class GURL;

namespace sync_file_system {

class MockRemoteFileSyncService : public RemoteFileSyncService {
 public:
  MockRemoteFileSyncService();
  virtual ~MockRemoteFileSyncService();

  // RemoteFileSyncService overrides.
  MOCK_METHOD1(AddObserver, void(RemoteFileSyncService::Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(RemoteFileSyncService::Observer* observer));
  MOCK_METHOD1(RegisterOriginForTrackingChanges, void(const GURL& origin));
  MOCK_METHOD1(UnregisterOriginForTrackingChanges, void(const GURL& origin));
  MOCK_METHOD2(ProcessRemoteChange,
               void(RemoteChangeProcessor* processor,
                    const fileapi::SyncCompletionCallback& callback));
  MOCK_METHOD0(GetLocalChangeProcessor, LocalChangeProcessor*());

  // Sets a mock local change processor. The value is returned by
  // the default action for GetLocalChangeProcessor.
  void set_local_change_processor(
      scoped_ptr<LocalChangeProcessor> processor) {
    local_change_processor_ = processor.Pass();
  }

 private:
  void ProcessRemoteChangeStub(RemoteChangeProcessor* processor,
                               const fileapi::SyncCompletionCallback& callback);

  scoped_ptr<LocalChangeProcessor> local_change_processor_;

  DISALLOW_COPY_AND_ASSIGN(MockRemoteFileSyncService);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_REMOTE_FILE_SYNC_SERVICE_H_
