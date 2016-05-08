// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.precache;

import android.content.Context;

import java.util.EnumSet;

/**
 * PrecacheController class with wakelocks mocked out.
 */
public class MockPrecacheController extends PrecacheController {
    public int acquiredLockCnt = 0;
    public int releasedLockCnt = 0;

    MockPrecacheController(Context context) {
        super(context);
    }

    @Override
    void startPrecachingAfterSyncInit() {
        super.startPrecaching();
    }

    @Override
    void acquirePrecachingWakeLock() {
        acquiredLockCnt++;
    }

    @Override
    void releasePrecachingWakeLock() {
        releasedLockCnt++;
    }

    @Override
    EnumSet<FailureReason> interruptionReasons(Context context) {
        return null;
    }

    @Override
    void recordFailureReasons(Context context) {}
}
