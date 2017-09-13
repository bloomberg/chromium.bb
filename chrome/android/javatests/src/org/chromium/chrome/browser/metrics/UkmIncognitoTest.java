// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.ui.base.PageTransition;

/**
 * Tests for UKM monitoring of incognito activity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
        "force-enable-metrics-reporting"})
public class UkmIncognitoTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String DEBUG_PAGE = "chrome://ukm";

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    public String getUkmState(Tab normalTab) throws Exception {
        mActivityTestRule.loadUrlInTab(
                DEBUG_PAGE, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, normalTab);
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                normalTab.getContentViewCore().getWebContents(),
                "document.getElementById('state').textContent");
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testUkmIncognito() throws Exception {
        Tab normalTab = mActivityTestRule.getActivity().getActivityTab();

        Assert.assertEquals("UKM State:", "\"True\"", getUkmState(normalTab));

        mActivityTestRule.newIncognitoTabFromMenu();

        Assert.assertEquals("UKM State:", "\"False\"", getUkmState(normalTab));
    }
}
