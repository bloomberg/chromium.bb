// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import android.os.SystemClock;

import org.chromium.chrome.browser.browserservices.BrowserServicesMetrics;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;

import java.util.concurrent.TimeUnit;

import javax.inject.Inject;

/**
 * Records how long Trusted Web Activities are used for.
 */
@ActivityScope
public class TrustedWebActivityOpenTimeRecorder implements PauseResumeWithNativeObserver {
    private long mOnResumeTimestampMs;

    @Inject
    TrustedWebActivityOpenTimeRecorder(ActivityLifecycleDispatcher lifecycleDispatcher) {
        lifecycleDispatcher.register(this);
    }

    @Override
    public void onResumeWithNative() {
        mOnResumeTimestampMs = SystemClock.elapsedRealtime();
    }

    @Override
    public void onPauseWithNative() {
        assert mOnResumeTimestampMs != 0;
        BrowserServicesMetrics.recordTwaOpenTime(
                SystemClock.elapsedRealtime() - mOnResumeTimestampMs, TimeUnit.MILLISECONDS);
        mOnResumeTimestampMs = 0;
    }
}
