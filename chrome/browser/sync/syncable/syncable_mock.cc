// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/syncable_mock.h"

#include "base/location.h"
#include "chrome/browser/sync/test/null_transaction_observer.h"

MockDirectory::MockDirectory() {
  InitKernelForTest("myk", &delegate_, syncable::NullTransactionObserver());
}

MockDirectory::~MockDirectory() {}

MockSyncableWriteTransaction::MockSyncableWriteTransaction(
    const tracked_objects::Location& from_here, Directory *directory)
    : WriteTransaction(from_here, syncable::UNITTEST, directory) {
}
