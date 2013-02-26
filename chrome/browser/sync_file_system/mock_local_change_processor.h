// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_LOCAL_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_LOCAL_CHANGE_PROCESSOR_H_

#include "base/basictypes.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_file_system {

class MockLocalChangeProcessor : public LocalChangeProcessor {
 public:
  MockLocalChangeProcessor();
  virtual ~MockLocalChangeProcessor();

  // LocalChangeProcessor override.
  MOCK_METHOD4(ApplyLocalChange,
               void(const FileChange& change,
                    const base::FilePath& local_file_path,
                    const fileapi::FileSystemURL& url,
                    const SyncStatusCallback& callback));

 private:
  void ApplyLocalChangeStub(
      const FileChange& change,
      const base::FilePath& local_file_path,
      const fileapi::FileSystemURL& url,
      const SyncStatusCallback& callback);

  DISALLOW_COPY_AND_ASSIGN(MockLocalChangeProcessor);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_MOCK_LOCAL_CHANGE_PROCESSOR_H_
