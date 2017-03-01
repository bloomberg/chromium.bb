// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.offlinepages.interfaces.BackgroundSchedulerProcessor;

/**
 * Custom stub for our own BackgroundSchedulerRequestProcessor.
 */
public class StubBackgroundSchedulerProcessor implements BackgroundSchedulerProcessor {
    private boolean mFailToStart;
    private boolean mDidStartProcessing;
    private DeviceConditions mDeviceConditions;
    private Callback<Boolean> mCallback;

    public void setFailToStart(boolean failToStart) {
        mFailToStart = failToStart;
    }

    public boolean getDidStartProcessing() {
        return mDidStartProcessing;
    }

    public DeviceConditions getDeviceConditions() {
        return mDeviceConditions;
    }

    public void callback() {
        mCallback.onResult(true);
    }

    @Override
    public boolean startScheduledProcessing(
            DeviceConditions deviceConditions, Callback<Boolean> callback) {
        if (mFailToStart) {
            return false;
        }

        mDidStartProcessing = true;
        mDeviceConditions = deviceConditions;
        mCallback = callback;
        return true;
    }

    @Override
    public boolean stopScheduledProcessing() {
        return true;
    }
}
