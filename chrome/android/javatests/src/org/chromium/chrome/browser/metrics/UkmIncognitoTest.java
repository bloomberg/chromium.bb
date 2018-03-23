// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.ui.base.PageTransition;

/**
 * Tests for UKM monitoring of incognito activity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-enable-metrics-reporting"})
public class UkmIncognitoTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String DEBUG_PAGE = "chrome://ukm";

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /*
     * These helper method should stay in sync with the tests within
     * sync_shell/.../chrome/browser/sync/UkmTest.java.
     */
    public String getElementContent(Tab normalTab, String elementId) throws Exception {
        mActivityTestRule.loadUrlInTab(
                DEBUG_PAGE, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, normalTab);
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                normalTab.getContentViewCore().getWebContents(),
                "document.getElementById('" + elementId + "').textContent");
    }

    public boolean isUkmEnabled(Tab normalTab) throws Exception {
        String state = getElementContent(normalTab, "state");
        Assert.assertTrue(
                "UKM state: " + state, state.equals("\"True\"") || state.equals("\"False\""));
        return state.equals("\"True\"");
    }

    public String getUkmClientId(Tab normalTab) throws Exception {
        return getElementContent(normalTab, "clientid");
    }

    /**
     * Closes the current tab.
     * @param incognito Whether to close an incognito or non-incognito tab.
     */
    protected void closeCurrentTab(final boolean incognito) throws InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivityTestRule.getActivity().getTabModelSelector().selectModel(incognito);
            }
        });
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
    }

    protected void closeRegularTab() throws InterruptedException {
        closeCurrentTab(false);
    }

    protected void closeIncognitoTab() throws InterruptedException {
        closeCurrentTab(true);
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testRegularPlusIncognitoCheck() throws Exception {
        // Keep in sync with UkmBrowserTest.RegularPlusIncognitoCheck in
        // chrome/browser/metrics/ukm_browsertest.cc.
        Tab normalTab = mActivityTestRule.getActivity().getActivityTab();

        Assert.assertTrue("UKM Enabled:", isUkmEnabled(normalTab));

        String clientId = getUkmClientId(normalTab);
        Assert.assertFalse("Non empty client id: " + clientId, clientId.isEmpty());

        mActivityTestRule.newIncognitoTabFromMenu();

        Assert.assertFalse("UKM Enabled:", isUkmEnabled(normalTab));

        // Opening another regular tab mustn't enable UKM.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Assert.assertFalse("UKM Enabled:", isUkmEnabled(normalTab));

        // Opening and closing another Incognito tab mustn't enable UKM.
        mActivityTestRule.newIncognitoTabFromMenu();
        closeIncognitoTab();
        Assert.assertFalse("UKM Enabled:", isUkmEnabled(normalTab));

        closeRegularTab();
        Assert.assertFalse("UKM Enabled:", isUkmEnabled(normalTab));

        closeIncognitoTab();
        Assert.assertTrue("UKM Enabled:", isUkmEnabled(normalTab));

        // Client ID should not have been reset.
        Assert.assertEquals("Client id:", clientId, getUkmClientId(normalTab));
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testIncognitoPlusRegularCheck() throws Exception {
        // Keep in sync with UkmBrowserTest.IncognitoPlusRegularCheck in
        // chrome/browser/metrics/ukm_browsertest.cc.

        // Start by closing all tabs.
        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        mActivityTestRule.newIncognitoTabFromMenu();

        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Tab normalTab = mActivityTestRule.getActivity().getActivityTab();

        Assert.assertFalse("UKM Enabled:", isUkmEnabled(normalTab));

        closeIncognitoTab();
        Assert.assertTrue("UKM Enabled:", isUkmEnabled(normalTab));
    }
}
