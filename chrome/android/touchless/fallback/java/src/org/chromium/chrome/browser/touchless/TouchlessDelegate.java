// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.native_page.NativePage;
import org.chromium.chrome.browser.native_page.NativePageHost;

/**
 * The fallback version of TouchlessDelegate, when touchless mode isn't enabled.
 */
public class TouchlessDelegate {
    public static final boolean TOUCHLESS_MODE_ENABLED = false;

    public static NativePage createTouchlessNewTabPage(
            ChromeActivity activity, NativePageHost host) {
        return null;
    }

    public static boolean isTouchlessNewTabPage(NativePage nativePage) {
        return false;
    }

    public static NativePage createTouchlessExploreSitesPage(
            ChromeActivity activity, NativePageHost host) {
        return null;
    }

    public static Class<?> getNoTouchActivityClass() {
        return null;
    }
}
