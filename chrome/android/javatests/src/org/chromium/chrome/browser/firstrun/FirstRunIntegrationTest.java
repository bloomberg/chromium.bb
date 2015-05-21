// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;

/**
 * Integration test suite for the first run experience.
 */
@CommandLineFlags.Remove(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class FirstRunIntegrationTest extends ChromeTabbedActivityTestBase {

    /**
     * Exiting the first run experience should close all tabs,
     * finalize the tab closures and close Chrome.
     */
    @SmallTest
    @Feature({"FirstRunExperience"})
    public void testExitFirstRunExperience() {
        if (FirstRunStatus.getFirstRunFlowComplete(getActivity())) {
            return;
        }

        sendKeys(KeyEvent.KEYCODE_BACK);
        getInstrumentation().waitForIdleSync();

        assertEquals("Expected no tabs to be present",
                0, getActivity().getCurrentTabModel().getCount());
        TabList fullModel = getActivity().getCurrentTabModel().getComprehensiveModel();
        assertEquals("Expected no tabs to be present in the comprehensive model",
                0, fullModel.getCount());
        assertTrue("Activity was not closed.",
                getActivity().isFinishing() || getActivity().isDestroyed());
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityFromLauncher();
    }

}
