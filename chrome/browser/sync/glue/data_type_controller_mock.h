// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_CONTROLLER_MOCK_H__
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_CONTROLLER_MOCK_H__
#pragma once

#include "chrome/browser/sync/glue/data_type_controller.h"
#include "sync/api/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class StartCallbackMock {
 public:
  StartCallbackMock();
  virtual ~StartCallbackMock();

  MOCK_METHOD2(Run, void(DataTypeController::StartResult result,
                         const csync::SyncError& error));
};

class ModelLoadCallbackMock {
 public:
  ModelLoadCallbackMock();
  virtual ~ModelLoadCallbackMock();

  MOCK_METHOD2(Run, void(syncable::ModelType, csync::SyncError));
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_CONTROLLER_MOCK_H__
