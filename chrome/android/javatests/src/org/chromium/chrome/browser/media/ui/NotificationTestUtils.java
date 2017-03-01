// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.app.Instrumentation;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Helper functions used in notification instrumentation tests.
 */
public class NotificationTestUtils {
    /**
     * Simulates a same page navigation.
     * @param instrumentation The Instrumentation object for the test.
     * @param tab The tab to be navigated.
     * @param url The original URL. The URL to be navigated to is |url| + "#some-anchor".
     */
    public static void simulateSamePageNavigation(
            Instrumentation instrumentation, final Tab tab, final String url) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.loadUrl(new LoadUrlParams(url + "#some-anchor"));
            }
        });
        instrumentation.waitForIdleSync();
    }
}
