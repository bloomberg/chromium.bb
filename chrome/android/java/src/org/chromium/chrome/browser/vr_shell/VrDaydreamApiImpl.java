// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Intent;

import com.google.vr.ndk.base.AndroidCompat;
import com.google.vr.ndk.base.DaydreamApi;

import org.chromium.base.annotations.UsedByReflection;

/**
 * A wrapper for DaydreamApi. Note that we have to recreate the DaydreamApi instance each time we
 * use it, or API calls begin to silently fail.
 */
@UsedByReflection("VrShellDelegate.java")
public class VrDaydreamApiImpl implements VrDaydreamApi {
    private final Activity mActivity;

    @UsedByReflection("VrShellDelegate.java")
    public VrDaydreamApiImpl(Activity activity) {
        mActivity = activity;
    }

    @Override
    public boolean isDaydreamReadyDevice() {
        return DaydreamApi.isDaydreamReadyPlatform(mActivity);
    }

    private void checkDaydreamReadyDevice() {
        if (!DaydreamApi.isDaydreamReadyPlatform(mActivity)) {
            throw new UnsupportedOperationException();
        }
    }

    @Override
    public void registerDaydreamIntent(final PendingIntent pendingIntent) {
        checkDaydreamReadyDevice();
        DaydreamApi daydreamApi = DaydreamApi.create(mActivity);
        daydreamApi.registerDaydreamIntent(pendingIntent);
        daydreamApi.close();
    }

    @Override
    public void unregisterDaydreamIntent() {
        checkDaydreamReadyDevice();
        DaydreamApi daydreamApi = DaydreamApi.create(mActivity);
        daydreamApi.unregisterDaydreamIntent();
        daydreamApi.close();
    }

    @Override
    public Intent createVrIntent(final ComponentName componentName) {
        return DaydreamApi.createVrIntent(componentName);
    }

    @Override
    public void launchInVr(final PendingIntent pendingIntent) {
        checkDaydreamReadyDevice();
        DaydreamApi daydreamApi = DaydreamApi.create(mActivity);
        daydreamApi.launchInVr(pendingIntent);
        daydreamApi.close();
    }

    @Override
    public void exitFromVr(int requestCode, final Intent intent) {
        checkDaydreamReadyDevice();
        DaydreamApi daydreamApi = DaydreamApi.create(mActivity);
        daydreamApi.exitFromVr(mActivity, requestCode, intent);
        daydreamApi.close();
    }

    @Override
    public void setVrModeEnabled(boolean enabled) {
        AndroidCompat.setVrModeEnabled(mActivity, enabled);
    }
}
