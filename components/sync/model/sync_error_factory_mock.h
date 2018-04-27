// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_ERROR_FACTORY_MOCK_H_
#define COMPONENTS_SYNC_MODEL_SYNC_ERROR_FACTORY_MOCK_H_

#include <string>

#include "components/sync/model/sync_error_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class SyncErrorFactoryMock : public SyncErrorFactory {
 public:
  SyncErrorFactoryMock();
  ~SyncErrorFactoryMock() override;

  MOCK_METHOD2(CreateAndUploadError,
               SyncError(const base::Location& location,
                         const std::string& message));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_ERROR_FACTORY_MOCK_H_
