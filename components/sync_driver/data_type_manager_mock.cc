// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "components/sync_driver/data_type_manager_mock.h"

namespace sync_driver {

DataTypeManagerMock::DataTypeManagerMock()
    : result_(OK, syncer::ModelTypeSet()) {
}

DataTypeManagerMock::~DataTypeManagerMock() {}

}  // namespace sync_driver
