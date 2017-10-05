// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.base.ApplicationState.HAS_DESTROYED_ACTIVITIES;
import static org.chromium.base.ApplicationState.HAS_PAUSED_ACTIVITIES;
import static org.chromium.base.ApplicationState.HAS_STOPPED_ACTIVITIES;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.tab.InterceptNavigationDelegateImpl;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;

/**
 * Tests web navigations originating from a WebappActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebappNavigationTest {
    private static final String YOUTUBE_URL = "https://www.youtube.com/watch?v=EYmjoW4vIX8";
    private static final String OFF_ORIGIN_URL = "https://www.google.com/";
    private static final String WEB_APP_PATH = "/chrome/test/data/banners/manifest_test_page.html";
    private static final String IN_SCOPE_PAGE_PATH =
            "/chrome/test/data/banners/manifest_no_service_worker.html";

    @Rule
    public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
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
    @RetryOnFailure
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
    @RetryOnFailure
    public void testOffScopeNewTabLinkOpensInCct() throws Exception {
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
    @RetryOnFailure
    public void testInScopeNewTabLinkOpensInCct() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN));
        addAnchor("testId", mActivityTestRule.getUrlFromTestServer(IN_SCOPE_PAGE_PATH), "_blank");
        DOMUtils.clickNode(
                mActivityTestRule.getActivity().getActivityTab().getContentViewCore(), "testId");
        CustomTabActivity customTab = waitFor(CustomTabActivity.class);
        mActivityTestRule.waitUntilIdle(customTab);
        Assert.assertTrue(
                mActivityTestRule.runJavaScriptCodeInCurrentTab("document.body.textContent")
                        .contains("Do-nothing page with a service worker"));
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
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
    @RetryOnFailure
    public void testInScopeNavigationStaysInWebapp() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());

        String otherPageUrl = mActivityTestRule.getUrlFromTestServer(IN_SCOPE_PAGE_PATH);
        mActivityTestRule.loadUrlInTab(otherPageUrl, PageTransition.LINK,
                mActivityTestRule.getActivity().getActivityTab());

        mActivityTestRule.waitUntilIdle();
        Assert.assertEquals(
                otherPageUrl, mActivityTestRule.getActivity().getActivityTab().getUrl());

        Assert.assertSame(
                mActivityTestRule.getActivity(), ApplicationStatus.getLastTrackedFocusedActivity());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testOpenInChromeFromContextMenuTabbedChrome() throws Exception {
        // Needed to get full context menu.
        FirstRunStatus.setFirstRunFlowComplete(true);
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());

        addAnchor("myTestAnchorId", OFF_ORIGIN_URL, "_self");

        ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                null /* activity to check for focus after click */,
                mActivityTestRule.getActivity().getActivityTab(), "myTestAnchorId",
                R.id.menu_id_open_in_chrome);

        ChromeTabbedActivity tabbedChrome = waitFor(ChromeTabbedActivity.class);

        mActivityTestRule.waitUntilIdle(tabbedChrome);
        // Dropping the TLD as Google can redirect to a local site, so this could fail outside US.
        Assert.assertTrue(tabbedChrome.getActivityTab().getUrl().startsWith("https://www.google."));
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testOpenInChromeFromCustomMenuTabbedChrome() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_DISPLAY_MODE, WebDisplayMode.MINIMAL_UI));

        WebappActivity activity = mActivityTestRule.getActivity();
        WebContents webAppWebContents = activity.getActivityTab().getWebContents();

        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), activity, R.id.open_in_browser_id);

        ChromeTabbedActivity tabbedChrome = waitFor(ChromeTabbedActivity.class);
        mActivityTestRule.waitUntilIdle(tabbedChrome);

        Assert.assertEquals("Tab in tabbed activity should show the Web App page",
                mActivityTestRule.getUrlFromTestServer(WEB_APP_PATH),
                tabbedChrome.getActivityTab().getUrl());
        Assert.assertSame("WebContents should be reparented from Web App to tabbed Chrome",
                webAppWebContents, tabbedChrome.getActivityTab().getWebContents());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testRegularLinkToExternalApp() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());

        InterceptNavigationDelegateImpl navigationDelegate =
                mActivityTestRule.getActivity().getActivityTab().getInterceptNavigationDelegate();

        addAnchor("testLink", YOUTUBE_URL, "_self");
        DOMUtils.clickNode(
                mActivityTestRule.getActivity().getActivityTab().getContentViewCore(), "testLink");

        waitForExternalAppOrIntentPicker();
        Assert.assertEquals(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT,
                navigationDelegate.getLastOverrideUrlLoadingResultForTests());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testNewTabLinkToExternalApp() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());

        addAnchor("testLink", YOUTUBE_URL, "_blank");
        DOMUtils.clickNode(
                mActivityTestRule.getActivity().getActivityTab().getContentViewCore(), "testLink");

        waitForExternalAppOrIntentPicker();
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    // Regression test for crbug.com/771174.
    public void testCanNavigateAfterReparentingToTabbedChrome() throws Exception {
        runWebappActivityAndWaitForIdle(
                mActivityTestRule.createIntent()
                        .putExtra(ShortcutHelper.EXTRA_DISPLAY_MODE, WebDisplayMode.MINIMAL_UI)
                        .putExtra(ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN));

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), R.id.open_in_browser_id);

        ChromeTabbedActivity tabbedChrome = waitFor(ChromeTabbedActivity.class);
        mActivityTestRule.waitUntilIdle(tabbedChrome);

        ThreadUtils.runOnUiThreadBlocking(
                () -> tabbedChrome.getActivityTab().loadUrl(new LoadUrlParams(OFF_ORIGIN_URL)));

        mActivityTestRule.waitUntilIdle(tabbedChrome);
        // Dropping the TLD as Google can redirect to a local site, so this could fail outside US.
        Assert.assertTrue(tabbedChrome.getActivityTab().getUrl().contains("https://www.google."));
    }

    private void runWebappActivityAndWaitForIdle(Intent intent) throws Exception {
        mActivityTestRule.startWebappActivity(intent.putExtra(
                ShortcutHelper.EXTRA_URL, mActivityTestRule.getUrlFromTestServer(WEB_APP_PATH)));

        mActivityTestRule.waitUntilSplashscreenHides();
        mActivityTestRule.waitUntilIdle();
    }

    private CustomTabActivity assertCustomTabActivityLaunchedForOffOriginUrl() {
        CustomTabActivity customTab = waitFor(CustomTabActivity.class);

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

    @SuppressWarnings("unchecked")
    private <T extends Activity> T waitFor(final Class<T> expectedClass) {
        final Activity[] holder = new Activity[1];
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                holder[0] = ApplicationStatus.getLastTrackedFocusedActivity();
                return holder[0] != null && expectedClass.isAssignableFrom(holder[0].getClass());
            }
        });
        return (T) holder[0];
    }

    private void waitForExternalAppOrIntentPicker() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ApplicationStatus.getStateForApplication() == HAS_PAUSED_ACTIVITIES
                        || ApplicationStatus.getStateForApplication() == HAS_STOPPED_ACTIVITIES
                        || ApplicationStatus.getStateForApplication() == HAS_DESTROYED_ACTIVITIES;
            }
        });
    }
}
