// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import android.webkit.ValueCallback;

import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.android_webview.test.AwTestBase;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.util.concurrent.Callable;

/**
 * This class provides common methods for AwQuotaManagerBridge related tests
 */
public class AwQuotaManagerBridgeTestUtil {

    public static AwQuotaManagerBridge getQuotaManagerBridge(AwTestBase awTestBase)
            throws Exception {
        return awTestBase.runTestOnUiThreadAndGetResult(new Callable<AwQuotaManagerBridge>() {
            @Override
            public AwQuotaManagerBridge call() throws Exception {
                return AwQuotaManagerBridge.getInstance();
            }
        });
    }

    private static class GetOriginsCallbackHelper extends CallbackHelper {
        private AwQuotaManagerBridge.Origins mOrigins;

        public void notifyCalled(AwQuotaManagerBridge.Origins origins) {
            mOrigins = origins;
            notifyCalled();
        }

        public AwQuotaManagerBridge.Origins getOrigins() {
            assert getCallCount() > 0;
            return mOrigins;
        }
    }

    public static AwQuotaManagerBridge.Origins getOrigins(AwTestBase awTestBase)
            throws Exception {
        final GetOriginsCallbackHelper callbackHelper = new GetOriginsCallbackHelper();
        final AwQuotaManagerBridge bridge = getQuotaManagerBridge(awTestBase);

        int callCount = callbackHelper.getCallCount();
        awTestBase.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                bridge.getOrigins(
                    new ValueCallback<AwQuotaManagerBridge.Origins>() {
                        @Override
                        public void onReceiveValue(AwQuotaManagerBridge.Origins origins) {
                            callbackHelper.notifyCalled(origins);
                        }
                    }
                );
            }
        });
        callbackHelper.waitForCallback(callCount);

        return callbackHelper.getOrigins();
    }

}
