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
    private boolean mStartProcessingCalled = false;

    public boolean getStartProcessingCalled() {
        return mStartProcessingCalled;
    }

    @Override
    public boolean startProcessing(Callback<Boolean> callback) {
        mStartProcessingCalled = true;
        return true;
    }
}
