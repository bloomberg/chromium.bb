// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Intent;

/**
 * Mock implementation of VR Shell's VrDaydreamApi
 */
public class MockVrDaydreamApi implements VrDaydreamApi {
    private boolean mLaunchInVrCalled;

    @Override
    public boolean isDaydreamReadyDevice() {
        return false;
    }

    @Override
    public boolean registerDaydreamIntent(final PendingIntent pendingIntent) {
        return false;
    }

    @Override
    public boolean unregisterDaydreamIntent() {
        return false;
    }

    @Override
    public Intent createVrIntent(final ComponentName componentName) {
        return new Intent();
    }

    @Override
    public boolean launchInVr(final PendingIntent pendingIntent) {
        mLaunchInVrCalled = true;
        return true;
    }

    @Override
    public boolean exitFromVr(int requestCode, final Intent intent) {
        return true;
    }

    @Override
    public Boolean isDaydreamCurrentViewer() {
        return false;
    }

    @Override
    public void launchVrHomescreen() {}

    public boolean getLaunchInVrCalled() {
        return mLaunchInVrCalled;
    }
}
