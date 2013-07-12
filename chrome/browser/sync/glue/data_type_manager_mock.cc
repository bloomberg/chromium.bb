// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/glue/data_type_manager_mock.h"

namespace browser_sync {

DataTypeManagerMock::DataTypeManagerMock()
    : result_(OK, syncer::ModelTypeSet()) {
}

DataTypeManagerMock::~DataTypeManagerMock() {}

}  // namespace browser_sync
