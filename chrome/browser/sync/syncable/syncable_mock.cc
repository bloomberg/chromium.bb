// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/syncable_mock.h"

#include "base/tracked.h"

MockDirectory::MockDirectory() {
  InitKernel("myk", &delegate_);
}

MockDirectory::~MockDirectory() {}

MockSyncableWriteTransaction::MockSyncableWriteTransaction(
    Directory *directory)
    : WriteTransaction(directory, syncable::UNITTEST, FROM_HERE) {
}
