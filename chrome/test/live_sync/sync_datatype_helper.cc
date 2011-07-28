// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/sync_datatype_helper.h"

#include "chrome/test/live_sync/live_sync_test.h"

SyncDatatypeHelper::SyncDatatypeHelper() {}

SyncDatatypeHelper::~SyncDatatypeHelper() {}

// static
void SyncDatatypeHelper::AssociateWithTest(LiveSyncTest* test) {
  ASSERT_TRUE(test != NULL) << "Cannot associate with null test.";
  ASSERT_TRUE(test_ == NULL) << "Already associated with a test.";
  test_ = test;
}

// static
LiveSyncTest* SyncDatatypeHelper::test() {
  EXPECT_TRUE(test_ != NULL) << "Must call AssociateWithTest first.";
  return test_;
}

// static
LiveSyncTest* SyncDatatypeHelper::test_ = NULL;
