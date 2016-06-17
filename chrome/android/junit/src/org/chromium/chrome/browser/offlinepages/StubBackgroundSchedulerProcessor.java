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
    private boolean mStartProcessingCalled;
    private DeviceConditions mDeviceConditions;

    public boolean getStartProcessingCalled() {
        return mStartProcessingCalled;
    }

    public DeviceConditions getDeviceConditions() {
        return mDeviceConditions;
    }

    @Override
    public boolean startProcessing(DeviceConditions deviceConditions, Callback<Boolean> callback) {
        mStartProcessingCalled = true;
        mDeviceConditions = deviceConditions;
        return true;
    }
}
