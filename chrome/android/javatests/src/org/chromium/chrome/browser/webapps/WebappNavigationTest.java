// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.Color;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;

/**
 * Tests web navigations originating from a WebappActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebappNavigationTest {
    private static final String OFF_ORIGIN_URL = "https://www.google.com/";
    private static final String WEB_APP_PATH = "/chrome/test/data/banners/manifest_test_page.html";
    private static final String OTHER_PAGE_PATH =
            "/chrome/test/data/banners/manifest_no_service_worker.html";

    @Rule
    public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    @Rule
    public final TopActivityListener activityListener = new TopActivityListener();

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testRegularLinkOffOriginInCctNoWebappThemeColor() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());
        addAnchor("testId", OFF_ORIGIN_URL, "_self");
        DOMUtils.clickNode(
                mActivityTestRule.getActivity().getActivityTab().getContentViewCore(), "testId");

        CustomTabActivity customTab = assertCustomTabActivityLaunchedForOffOriginUrl();

        Assert.assertEquals(
                "CCT Toolbar should use default primary color if theme color is not specified",
                ApiCompatibilityUtils.getColor(
                        customTab.getResources(), R.color.default_primary_color),
                customTab.getToolbarManager().getPrimaryColor());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testWindowTopLocationOffOriginInCctAndWebappThemeColor() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN));

        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                String.format("window.top.location = '%s'", OFF_ORIGIN_URL));

        CustomTabActivity customTab = assertCustomTabActivityLaunchedForOffOriginUrl();

        Assert.assertEquals("CCT Toolbar should use the theme color of a webapp", Color.CYAN,
                customTab.getToolbarManager().getPrimaryColor());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testNewTabLinkOpensInCct() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN));
        addAnchor("testId", OFF_ORIGIN_URL, "_blank");
        DOMUtils.clickNode(
                mActivityTestRule.getActivity().getActivityTab().getContentViewCore(), "testId");
        CustomTabActivity customTab = assertCustomTabActivityLaunchedForOffOriginUrl();
        Assert.assertEquals(
                "CCT Toolbar should use default primary color even if webapp has theme color",
                ApiCompatibilityUtils.getColor(
                        customTab.getResources(), R.color.default_primary_color),
                customTab.getToolbarManager().getPrimaryColor());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testWindowOpenInCct() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());
        // Executing window.open() through a click on a link,
        // as it needs user gesture to avoid Chrome blocking it as a popup.
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                String.format("var aTag = document.createElement('testId');"
                                + "aTag.id = 'testId';"
                                + "aTag.innerHTML = 'Click Me!';"
                                + "aTag.onclick = function() {"
                                + "  window.open('%s');"
                                + "  return false;"
                                + "};"
                                + "document.body.appendChild(aTag);",
                        OFF_ORIGIN_URL));
        DOMUtils.clickNode(
                mActivityTestRule.getActivity().getActivityTab().getContentViewCore(), "testId");

        CustomTabActivity customTab = assertCustomTabActivityLaunchedForOffOriginUrl();
        Assert.assertEquals("CCT Toolbar should use default primary color",
                ApiCompatibilityUtils.getColor(
                        customTab.getResources(), R.color.default_primary_color),
                customTab.getToolbarManager().getPrimaryColor());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testInOriginNavigationStaysInWebapp() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());

        String otherPageUrl = mTestServer.getURL(OTHER_PAGE_PATH);
        mActivityTestRule.loadUrlInTab(otherPageUrl, PageTransition.LINK,
                mActivityTestRule.getActivity().getActivityTab());

        mActivityTestRule.waitUntilIdle();
        Assert.assertEquals(
                otherPageUrl, mActivityTestRule.getActivity().getActivityTab().getUrl());

        Assert.assertSame(
                mActivityTestRule.getActivity(), activityListener.getMostRecentActivity());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testOpenInChromeFromContextMenuTabbedChrome() throws Exception {
        // Needed to get full context menu.
        FirstRunStatus.setFirstRunFlowComplete(true);
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());

        addAnchor("myTestAnchorId", OFF_ORIGIN_URL, "_self");

        ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                null /* activity to check for focus after click */,
                mActivityTestRule.getActivity().getActivityTab(), "myTestAnchorId",
                R.id.menu_id_open_in_chrome);

        ChromeTabbedActivity tabbedChrome = activityListener.waitFor(ChromeTabbedActivity.class);

        mActivityTestRule.waitUntilIdle(tabbedChrome);
        // Dropping the TLD as Google can redirect to a local site, so this could fail outside US.
        Assert.assertTrue(tabbedChrome.getActivityTab().getUrl().startsWith("https://www.google."));
    }

    private void runWebappActivityAndWaitForIdle(Intent intent) throws Exception {
        mActivityTestRule.startWebappActivity(
                intent.putExtra(ShortcutHelper.EXTRA_URL, mTestServer.getURL(WEB_APP_PATH)));

        mActivityTestRule.waitUntilSplashscreenHides();
        mActivityTestRule.waitUntilIdle();
    }

    private CustomTabActivity assertCustomTabActivityLaunchedForOffOriginUrl() {
        CustomTabActivity customTab = activityListener.waitFor(CustomTabActivity.class);

        mActivityTestRule.waitUntilIdle(customTab);
        // Dropping the TLD as Google can redirect to a local site, so this could fail outside US.
        Assert.assertTrue(customTab.getActivityTab().getUrl().contains("https://www.google."));

        return customTab;
    }

    private void addAnchor(String id, String url, String target) throws Exception {
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                String.format("var aTag = document.createElement('a');"
                                + "aTag.id = '%s';"
                                + "aTag.setAttribute('href','%s');"
                                + "aTag.setAttribute('target','%s');"
                                + "aTag.innerHTML = 'Click Me!';"
                                + "document.body.appendChild(aTag);",
                        id, url, target));
    }
}
