// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell.mock;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Intent;

import com.google.vr.ndk.base.DaydreamApi;

import org.chromium.chrome.browser.vr_shell.VrDaydreamApi;

/**
 * Mock implementation of VR Shell's VrDaydreamApi
 */
public class MockVrDaydreamApi implements VrDaydreamApi {
    private boolean mLaunchInVrCalled;
    private boolean mExitFromVrCalled;
    private boolean mLaunchVrHomescreenCalled;
    private boolean mForwardSetupIntent;

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
    public boolean launchInVr(final Intent intent) {
        return true;
    }

    @Override
    public boolean exitFromVr(int requestCode, final Intent intent) {
        mExitFromVrCalled = true;
        return false;
    }

    @Override
    public Boolean isDaydreamCurrentViewer() {
        return false;
    }

    @Override
    public boolean launchVrHomescreen() {
        mLaunchVrHomescreenCalled = true;
        return true;
    }

    public boolean getLaunchInVrCalled() {
        return mLaunchInVrCalled;
    }

    public boolean getExitFromVrCalled() {
        return mExitFromVrCalled;
    }

    public boolean getLaunchVrHomescreenCalled() {
        return mLaunchVrHomescreenCalled;
    }

    @Override
    public boolean bootsToVr() {
        return false;
    }

    @Override
    public Intent setupVrIntent(Intent intent) {
        if (mForwardSetupIntent) {
            return DaydreamApi.setupVrIntent(intent);
        }
        return null;
    }

    public void setForwardSetupIntent(boolean forward) {
        mForwardSetupIntent = forward;
    }

    @Override
    public void launchGvrSettings() {}

    @Override
    public boolean isInVrSession() {
        return true;
    }

    @Override
    public void close() {}
}
