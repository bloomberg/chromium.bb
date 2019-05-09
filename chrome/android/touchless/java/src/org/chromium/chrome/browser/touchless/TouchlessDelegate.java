// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.native_page.NativePage;
import org.chromium.chrome.browser.native_page.NativePageHost;

/**
 *  Provides an entry point into touchless code from the rest of Chrome.
 */
public class TouchlessDelegate {
    public static final boolean TOUCHLESS_MODE_ENABLED = true;

    public static NativePage createTouchlessNewTabPage(
            ChromeActivity activity, NativePageHost host) {
        return new TouchlessNewTabPage(activity, host);
    }

    public static boolean isTouchlessNewTabPage(NativePage nativePage) {
        return nativePage instanceof TouchlessNewTabPage;
    }

    public static NativePage createTouchlessExploreSitesPage(
            ChromeActivity activity, NativePageHost host) {
        return new TouchlessExploreSitesPage(activity, host);
    }

    public static Class<?> getNoTouchActivityClass() {
        return NoTouchActivity.class;
    }
}
