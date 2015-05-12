// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.shell.ChromeShellTestBase;

/**
 * Tests related to the sad tab logic.
 */
public class SadTabTest extends ChromeShellTestBase {

    /**
     * Verify that the sad tab is shown when the renderer crashes.
     */
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabShownWhenRendererProcessKilled() {
        launchChromeShellWithBlankPage();
        final Tab tab = getActivity().getActiveTab();

        assertFalse(tab.isShowingSadTab());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.simulateRendererKilledForTesting(true);
            }
        });

        assertTrue(tab.isShowingSadTab());
    }

    /**
     * Verify that the sad tab is not shown when the renderer crashes in the background or the
     * renderer was killed by the OS out-of-memory killer.
     */
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabNotShownWhenRendererProcessKilledInBackround() {
        launchChromeShellWithBlankPage();
        final Tab tab = getActivity().getActiveTab();

        assertFalse(tab.isShowingSadTab());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.simulateRendererKilledForTesting(false);
            }
        });

        assertFalse(tab.isShowingSadTab());
    }
}
