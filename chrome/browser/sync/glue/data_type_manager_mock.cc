// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "chrome/browser/sync/glue/data_type_manager_mock.h"
#include "chrome/common/chrome_notification_types.h"

namespace browser_sync {

DataTypeManagerMock::DataTypeManagerMock()
    : result_(OK, syncable::ModelTypeSet()) {

  // By default, calling Configure will send a SYNC_CONFIGURE_START
  // and SYNC_CONFIGURE_DONE notification with a DataTypeManager::OK
  // detail.
    ON_CALL(*this, Configure(testing::_, testing::_)).
    WillByDefault(testing::DoAll(
        NotifyFromDataTypeManager(
            this,
            static_cast<int>(chrome::NOTIFICATION_SYNC_CONFIGURE_START)),
        NotifyFromDataTypeManagerWithResult(
            this, static_cast<int>(chrome::NOTIFICATION_SYNC_CONFIGURE_DONE),
            &result_)));

  // By default, calling ConfigureWithoutNigori will send a SYNC_CONFIGURE_START
  // and SYNC_CONFIGURE_DONE notification with a DataTypeManager::OK
  // detail.
  ON_CALL(*this, ConfigureWithoutNigori(testing::_, testing::_)).
  WillByDefault(testing::DoAll(
      NotifyFromDataTypeManager(
          this,
          static_cast<int>(chrome::NOTIFICATION_SYNC_CONFIGURE_START)),
      NotifyFromDataTypeManagerWithResult(
          this,
          static_cast<int>(chrome::NOTIFICATION_SYNC_CONFIGURE_DONE),
          &result_)));
}

DataTypeManagerMock::~DataTypeManagerMock() {}

}  // namespace browser_sync
