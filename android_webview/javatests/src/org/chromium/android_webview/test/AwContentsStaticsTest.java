// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;

import org.chromium.android_webview.AwContentsStatics;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;

/**
 * AwContentsStatics tests.
 */
public class AwContentsStaticsTest extends AwTestBase {

    private static class ClearClientCertCallbackHelper extends CallbackHelper
            implements Runnable {
        @Override
        public void run() {
            notifyCalled();
        }
    }

    @Feature({"AndroidWebView"})
    @SmallTest
    public void testClearClientCertPreferences() throws Throwable {
        final ClearClientCertCallbackHelper callbackHelper = new ClearClientCertCallbackHelper();
        int currentCallCount = callbackHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                // Make sure calling clearClientCertPreferences with null callback does not
                // cause a crash.
                AwContentsStatics.clearClientCertPreferences(null);
                AwContentsStatics.clearClientCertPreferences(callbackHelper);
            }
        });
        callbackHelper.waitForCallback(currentCallCount);
    }
}
