// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.support.test.filters.SmallTest;
import android.widget.Button;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests related to the sad tab logic.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SadTabTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Verify that the sad tab is shown when the renderer crashes.
     */
    @Test
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabShownWhenRendererProcessKilled() {
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        Assert.assertFalse(tab.isShowingSadTab());
        simulateRendererKilled(tab, true);
        Assert.assertTrue(tab.isShowingSadTab());
    }

    /**
     * Verify that the sad tab is not shown when the renderer crashes in the background or the
     * renderer was killed by the OS out-of-memory killer.
     */
    @Test
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabNotShownWhenRendererProcessKilledInBackround() {
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        Assert.assertFalse(tab.isShowingSadTab());
        simulateRendererKilled(tab, false);
        Assert.assertFalse(tab.isShowingSadTab());
    }

    /**
     * Confirm that after a successive refresh of a failed tab that failed to load, change the
     * button from "Reload" to "Send Feedback". If reloaded a third time and it is successful it
     * reverts from "Send Feedback" to "Reload".
     * @throws InterruptedException
     * @throws IllegalArgumentException
     */
    @Test
    @SmallTest
    @Feature({"SadTab"})
    public void testSadTabPageButtonText() throws IllegalArgumentException, InterruptedException {
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        Assert.assertFalse(tab.isShowingSadTab());
        simulateRendererKilled(tab, true);
        Assert.assertTrue(tab.isShowingSadTab());
        String actualText = getSadTabButton(tab).getText().toString();
        Assert.assertEquals("Expected the sad tab button to have the reload label",
                mActivityTestRule.getActivity().getString(R.string.sad_tab_reload_label),
                actualText);

        reloadSadTab(tab);
        Assert.assertTrue(tab.isShowingSadTab());
        actualText = getSadTabButton(tab).getText().toString();
        Assert.assertEquals(
                "Expected the sad tab button to have the feedback label after the tab button "
                        + "crashes twice in a row.",
                mActivityTestRule.getActivity().getString(R.string.sad_tab_send_feedback_label),
                actualText);
        mActivityTestRule.loadUrl("about:blank");
        Assert.assertFalse(
                "Expected about:blank to destroy the sad tab however the sad tab is still in "
                        + "view",
                tab.isShowingSadTab());
        simulateRendererKilled(tab, true);
        actualText = getSadTabButton(tab).getText().toString();
        Assert.assertEquals(
                "Expected the sad tab button to have the reload label after a successful load",
                mActivityTestRule.getActivity().getString(R.string.sad_tab_reload_label),
                actualText);
    }

    /**
     * Helper method that kills the renderer on a UI thread.
     */
    private void simulateRendererKilled(final Tab tab, final boolean wasOomProtected) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.simulateRendererKilledForTesting(wasOomProtected);
            }
        });
    }

    /**
     * Helper method that reloads a tab with a SadTabView currently displayed.
     */
    private void reloadSadTab(final Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.reloadSadTabForTesting();
            }
        });
    }

    /**
     * If there is a SadTabView, this method will get the button for the sad tab.
     * @param tab The tab that needs to contain a SadTabView.
     * @return Returns the button that is on the SadTabView, null if SadTabView.
     *         doesn't exist.
     */
    private Button getSadTabButton(Tab tab) {
        return (Button) tab.getContentViewCore().getContainerView().findViewById(
                R.id.sad_tab_button);
    }

}
