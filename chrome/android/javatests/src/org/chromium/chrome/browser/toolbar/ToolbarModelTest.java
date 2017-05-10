// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;

/**
 * Tests for ToolbarModel.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class ToolbarModelTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * After closing all {@link Tab}s, the {@link ToolbarModelImpl} should know that it is not
     * showing any {@link Tab}.
     * @throws InterruptedException
     */
    @Test
    @Feature({"Android-Toolbar"})
    @MediumTest
    @RetryOnFailure
    public void testClosingLastTabReflectedInModel() throws InterruptedException {
        Assert.assertNotSame("No current tab", Tab.INVALID_TAB_ID,
                getCurrentTabId(mActivityTestRule.getActivity()));
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Assert.assertEquals("Didn't close all tabs.", 0,
                ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity()));
        Assert.assertEquals("ToolbarModel is still trying to show a tab.", Tab.INVALID_TAB_ID,
                getCurrentTabId(mActivityTestRule.getActivity()));
    }

    /**
     * @param activity A reference to {@link ChromeTabbedActivity} to pull
     *            {@link android.view.View} data from.
     * @return The id of the current {@link Tab} as far as the {@link ToolbarModelImpl} sees it.
     */
    public static int getCurrentTabId(final ChromeTabbedActivity activity) {
        ToolbarLayout toolbar = (ToolbarLayout) activity.findViewById(R.id.toolbar);
        Assert.assertNotNull("Toolbar is null", toolbar);

        ToolbarDataProvider dataProvider = toolbar.getToolbarDataProvider();
        Tab tab = dataProvider.getTab();
        return tab != null ? tab.getId() : Tab.INVALID_TAB_ID;
    }
}
