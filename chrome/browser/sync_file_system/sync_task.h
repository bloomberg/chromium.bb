// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_TASK_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_TASK_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"

namespace sync_file_system {

class SyncTaskToken;

class SyncTask {
 public:
  SyncTask() : used_network_(false) {}
  virtual ~SyncTask() {}
  virtual void Run(scoped_ptr<SyncTaskToken> token) = 0;

  bool used_network() { return used_network_; }

 protected:
  void set_used_network(bool used_network) {
    used_network_ = used_network;
  }

 private:
  bool used_network_;

  DISALLOW_COPY_AND_ASSIGN(SyncTask);
};

class SequentialSyncTask : public SyncTask {
 public:
  virtual void Run(scoped_ptr<SyncTaskToken> token) OVERRIDE FINAL;
  virtual void RunSequential(const SyncStatusCallback& callback) = 0;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_TASK_H_
