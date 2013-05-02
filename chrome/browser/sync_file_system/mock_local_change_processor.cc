// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/mock_local_change_processor.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace sync_file_system {

MockLocalChangeProcessor::MockLocalChangeProcessor() {
}

MockLocalChangeProcessor::~MockLocalChangeProcessor() {
}

}  // namespace sync_file_system
