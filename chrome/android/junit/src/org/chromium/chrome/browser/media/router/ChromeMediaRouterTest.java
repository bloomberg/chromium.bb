// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.app.ActivityManager;
import android.os.Build;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowActivityManager;

/**
 * Robolectric tests for ChromeMediaRouter.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = { ChromeMediaRouterTest.FakeActivityManager.class })
public class ChromeMediaRouterTest {

    private static boolean sIsLowRamDevice;
    private ChromeMediaRouter mChromeMediaRouter;

    /**
     * Robolectric's ShadowActivityManager implementation in order to extend
     * isLowRamDevice and be able to instrument the tests.
     */
    @Implements(ActivityManager.class)
    public static class FakeActivityManager extends ShadowActivityManager {
        @Implementation
        public boolean isLowRamDevice() {
            return sIsLowRamDevice;
        }
    }

    @Before
    public void setUp() {
        sIsLowRamDevice = false;
        mChromeMediaRouter = new ChromeMediaRouter(0, Robolectric.application);

        // Initialize the command line to avoid an assertion failure in SysUtils.
        CommandLine.init(new String[0]);
    }

    @Test
    @Feature({"MediaRouter"})
    public void testNotLowRamDevice() {
        sIsLowRamDevice = false;
        assertTrue(mChromeMediaRouter.startObservingMediaSinks(""));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testIsLowRamDevice() {
        sIsLowRamDevice = true;
        assertEquals(Build.VERSION.SDK_INT <= Build.VERSION_CODES.JELLY_BEAN_MR2,
                     mChromeMediaRouter.startObservingMediaSinks(""));
    }
}
