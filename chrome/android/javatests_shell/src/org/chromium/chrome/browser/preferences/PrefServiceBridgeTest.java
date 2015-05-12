// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.shell.ChromeShellTestBase;

/**
 * Test for PrefServiceBridge
 */
public class PrefServiceBridgeTest extends ChromeShellTestBase {
    /**
     * Test the x-auto-login setting.
     */
    @SmallTest
    @Feature({"Preferences", "Main"})
    public void testAutologinEnabled() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PrefServiceBridge prefs = PrefServiceBridge.getInstance();

                boolean autologinEnabled = prefs.isAutologinEnabled();

                // Make sure the value doesn't change
                prefs.setAutologinEnabled(autologinEnabled);
                assertEquals(autologinEnabled, prefs.isAutologinEnabled());

                // Try flipping the value
                autologinEnabled = !autologinEnabled;
                prefs.setAutologinEnabled(autologinEnabled);
                assertEquals(autologinEnabled, prefs.isAutologinEnabled());
            }
        });
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        launchChromeShellWithBlankPage();
    }
}
