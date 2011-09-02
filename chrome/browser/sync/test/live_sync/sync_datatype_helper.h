// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_LIVE_SYNC_SYNC_DATATYPE_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_LIVE_SYNC_SYNC_DATATYPE_HELPER_H_
#pragma once

#include "base/basictypes.h"

class LiveSyncTest;

namespace sync_datatype_helper {

// Associates an instance of LiveSyncTest with sync_datatype_helper. Must be
// called before any of the methods in the per-datatype helper namespaces can be
// used.
void AssociateWithTest(LiveSyncTest* test);

// Returns a pointer to the instance of LiveSyncTest associated with the
// per-datatype helpers after making sure it is valid.
LiveSyncTest* test();

}  // namespace sync_datatype_helper

#endif  // CHROME_BROWSER_SYNC_TEST_LIVE_SYNC_SYNC_DATATYPE_HELPER_H_
