// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import android.support.test.InstrumentationRegistry;
import android.webkit.ValueCallback;

import org.junit.Assert;

import org.chromium.android_webview.AwCookieManager;
import org.chromium.base.test.util.CallbackHelper;

/**
 * Useful functions for testing the CookieManager.
 */
public class CookieUtils {
    private CookieUtils() {
    }


    /**
     * A CallbackHelper for use with setCookie/removeXXXCookie.
     */
    public static class TestValueCallback<T> implements ValueCallback<T> {
        /**
         * We only have one intresting method on ValueCallback: onReceiveValue.
         */
        public static class OnReceiveValueHelper<T> extends CallbackHelper {
            private T mValue;

            public T getValue() {
                assert getCallCount() > 0;
                return mValue;
            }

            public void notifyCalled(T value) {
                mValue = value;
                notifyCalled();
            }
        }

        private OnReceiveValueHelper<T> mOnReceiveValueHelper;

        public TestValueCallback() {
            mOnReceiveValueHelper = new OnReceiveValueHelper<T>();
        }

        public OnReceiveValueHelper getOnReceiveValueHelper() {
            return mOnReceiveValueHelper;
        }

        @Override
        public void onReceiveValue(T value) {
            mOnReceiveValueHelper.notifyCalled(value);
        }

        public T getValue() {
            return mOnReceiveValueHelper.getValue();
        }
    }

    /**
     * Clear all cookies from the CookieManager synchronously then assert they are gone.
     * @param  cookieManager the CookieManager on which to remove cookies.
     */
    public static void clearCookies(final AwCookieManager cookieManager) throws Throwable {
        final TestValueCallback<Boolean> callback = new TestValueCallback<Boolean>();
        int callCount = callback.getOnReceiveValueHelper().getCallCount();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> cookieManager.removeAllCookies(callback));
        callback.getOnReceiveValueHelper().waitForCallback(callCount);
        Assert.assertFalse(cookieManager.hasCookies());
    }
}
