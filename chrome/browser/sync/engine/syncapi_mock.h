// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCAPI_MOCK_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCAPI_MOCK_H_
#pragma once

#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_api::WriteTransaction;

class MockWriteTransaction : public sync_api::WriteTransaction {
 public:
  explicit MockWriteTransaction(Directory* directory)
     : sync_api::WriteTransaction() {
    this->SetTransaction(new MockSyncableWriteTransaction(directory));
  }
};

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCAPI_MOCK_H_

