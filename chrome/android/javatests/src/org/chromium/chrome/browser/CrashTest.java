// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;

/**
 * The class contains a testcase that intentionally crashes.
 */
public class CrashTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    public CrashTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() {
        // Don't launch activity automatically.
    }

    /**
     * Intentionally crashing the test. The testcase should be
     * disabled the majority of time.
     */
    @DisabledTest
    @SmallTest
    public void testIntentionalBrowserCrash() throws Exception {
        startMainActivityFromLauncher();
        loadUrl("chrome://inducebrowsercrashforrealz");
    }
}
