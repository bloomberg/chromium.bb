// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_MOCK_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_MOCK_H_
#pragma once

#include <string>

#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/test/null_directory_change_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncable::Directory;
using syncable::EntryKernel;

class MockDirectory : public Directory {
 public:
  MockDirectory();
  virtual ~MockDirectory();

  MOCK_METHOD1(GetEntryByHandle, syncable::EntryKernel*(int64));

  MOCK_METHOD2(set_last_downloadstamp, void(syncable::ModelType, int64));

  MOCK_METHOD1(GetEntryByClientTag,
               syncable::EntryKernel*(const std::string&));

 private:
  syncable::NullDirectoryChangeDelegate delegate_;
};

class MockSyncableWriteTransaction : public syncable::WriteTransaction {
 public:
  MockSyncableWriteTransaction(
      const tracked_objects::Location& from_here, Directory *directory);
};


#endif  // CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_MOCK_H_

