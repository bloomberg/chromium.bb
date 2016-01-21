// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/browser/profile_sync_test_util.h"

namespace browser_sync {

void EmptyNetworkTimeUpdate(const base::Time&,
                            const base::TimeDelta&,
                            const base::TimeDelta&) {}

SyncServiceObserverMock::SyncServiceObserverMock() {}

SyncServiceObserverMock::~SyncServiceObserverMock() {}

}  // namespace browser_sync
