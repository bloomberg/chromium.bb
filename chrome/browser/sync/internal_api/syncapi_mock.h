// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_INTERNAL_API_SYNCAPI_MOCK_H_
#define CHROME_BROWSER_SYNC_INTERNAL_API_SYNCAPI_MOCK_H_
#pragma once

#include "chrome/browser/sync/internal_api/write_transaction.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_api::WriteTransaction;

class MockWriteTransaction : public sync_api::WriteTransaction {
 public:
  MockWriteTransaction(const tracked_objects::Location& from_here,
                       Directory* directory)
     : sync_api::WriteTransaction() {
    SetTransaction(new MockSyncableWriteTransaction(from_here, directory));
  }
};

#endif  // CHROME_BROWSER_SYNC_INTERNAL_API_SYNCAPI_MOCK_H_

