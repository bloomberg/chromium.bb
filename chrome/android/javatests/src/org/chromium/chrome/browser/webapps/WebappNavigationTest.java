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
import android.support.test.filters.LargeTest;
import android.support.test.filters.SmallTest;
import android.util.Base64;

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
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.tab.InterceptNavigationDelegateImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.WebappTestPage;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content.browser.test.NativeLibraryTestRule;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.common.ContentSwitches;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;

/**
 * Tests web navigations originating from a WebappActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
public class WebappNavigationTest {
    private static final String YOUTUBE_URL = "https://www.youtube.com/watch?v=EYmjoW4vIX8";

    @Rule
    public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    @Rule
    public final NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    /**
     * Test that navigating a webapp whose launch intent does not specify a theme colour outside of
     * the webapp scope by tapping a regular link:
     * - Shows a CCT-like webapp toolbar.
     * - Uses the default theme colour as the toolbar colour.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testRegularLinkOffOriginNoWebappThemeColor() throws Exception {
        WebappActivity activity = runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());
        assertToolbarShowState(activity, false);

        addAnchorAndClick(offOriginUrl(), "_self");

        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());
        assertToolbarShowState(activity, true);
        Assert.assertEquals(
                getDefaultPrimaryColor(), activity.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating a webapp whose launch intent specifies a theme colour outside of the
     * webapp scope by tapping a regular link:
     * - Shows a CCT-like webapp toolbar.
     * - Uses the webapp theme colour as the toolbar colour.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testRegularLinkOffOriginThemeColor() throws Exception {
        WebappActivity activity =
                runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                        ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN));
        assertToolbarShowState(activity, false);

        addAnchorAndClick(offOriginUrl(), "_self");

        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());
        assertToolbarShowState(activity, true);
        Assert.assertEquals(Color.CYAN, activity.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating a TWA outside of the TWA scope by tapping a regular link:
     * - Launches a CCT.
     * - Uses the TWA theme colour in the CCT toolbar.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testRegularLinkOffOriginTwa() throws Exception {
        Intent launchIntent = mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN);
        mActivityTestRule.addTwaExtrasToIntent(launchIntent);
        WebappActivity activity = runWebappActivityAndWaitForIdle(launchIntent);

        addAnchorAndClick(offOriginUrl(), "_self");

        CustomTabActivity customTab = waitFor(CustomTabActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(customTab.getActivityTab(), offOriginUrl());
        Assert.assertEquals(Color.CYAN, customTab.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating outside of the webapp scope by changing the top location via JavaScript:
     * - Shows a CCT-like webapp toolbar.
     * - Preserves the theme color specified in the launch intent.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testWindowTopLocationOffOrigin() throws Exception {
        WebappActivity activity =
                runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                        ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN));
        assertToolbarShowState(activity, false);

        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                String.format("window.top.location = '%s'", offOriginUrl()));

        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());
        assertToolbarShowState(activity, true);
        Assert.assertEquals(Color.CYAN, activity.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating a TWA outside of the TWA scope by changing the top location via
     * JavaScript:
     * - Launches a CCT.
     * - Uses the TWA theme colour in the CCT toolbar.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testWindowTopLocationOffOriginTwa() throws Exception {
        Intent launchIntent = mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN);
        mActivityTestRule.addTwaExtrasToIntent(launchIntent);
        WebappActivity activity = runWebappActivityAndWaitForIdle(launchIntent);

        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                String.format("window.top.location = '%s'", offOriginUrl()));

        CustomTabActivity customTab = waitFor(CustomTabActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(customTab.getActivityTab(), offOriginUrl());
        Assert.assertEquals(Color.CYAN, customTab.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating outside of the webapp scope as a result of submitting a form with method
     * "POST":
     * - Shows a CCT-like webapp toolbar.
     * - Preserves the theme color specified in the launch intent.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testFormSubmitOffOrigin() throws Exception {
        Intent launchIntent = mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN);
        mActivityTestRule.addTwaExtrasToIntent(launchIntent);

        WebappActivity activity = runWebappActivityAndWaitForIdle(launchIntent);

        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                String.format("var formTag = document.createElement('form');"
                                + "formTag.id = 'post_form';"
                                + "formTag.setAttribute('method', 'post');"
                                + "formTag.setAttribute('action', '%s');"
                                + "document.body.appendChild(formTag);"
                                + "var buttonTag = document.createElement('input');"
                                + "buttonTag.id = 'post_button';"
                                + "buttonTag.setAttribute('type', 'submit');"
                                + "formTag.appendChild(buttonTag);",
                        offOriginUrl()));
        clickNodeWithId("post_button");

        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), offOriginUrl());
        Assert.assertEquals(Color.CYAN, activity.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating outside of the webapp scope by tapping a link with target="_blank":
     * - Launches a CCT.
     * - The CCT toolbar does not use the webapp theme colour.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testOffScopeNewTabLinkOpensInCct() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN));
        addAnchorAndClick(offOriginUrl(), "_blank");
        CustomTabActivity customTab = waitFor(CustomTabActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(customTab.getActivityTab(), offOriginUrl());

        Assert.assertEquals(
                getDefaultPrimaryColor(), customTab.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating within the webapp scope by tapping a link with target="_blank" launches
     * a CCT.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testInScopeNewTabLinkOpensInCct() throws Exception {
        String inScopeUrl =
                WebappTestPage.getNonServiceWorkerUrl(mActivityTestRule.getTestServer());
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN));
        addAnchorAndClick(inScopeUrl, "_blank");
        CustomTabActivity customTab = waitFor(CustomTabActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(customTab.getActivityTab(), inScopeUrl);
        Assert.assertTrue(
                mActivityTestRule.runJavaScriptCodeInCurrentTab("document.body.textContent")
                        .contains("Do-nothing page with a manifest and a service worker."));
    }

    /**
     * Test that navigating outside of the webapp via window.open():
     * - Launches a CCT.
     * - The CCT toolbar does not use the webapp theme colour.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testWindowOpenInCct() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) Color.CYAN));
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
                        offOriginUrl()));
        clickNodeWithId("testId");

        CustomTabActivity customTab = waitFor(CustomTabActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(customTab.getActivityTab(), offOriginUrl());
        Assert.assertEquals(
                getDefaultPrimaryColor(), customTab.getToolbarManager().getPrimaryColor());
    }

    /**
     * Test that navigating a webapp within the webapp scope by tapping a regular link:
     * - Does not show a CCT-like webapp toolbar.
     * - Does not launch a CCT.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testInScopeNavigationStaysInWebapp() throws Exception {
        WebappActivity activity = runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());
        String otherPageUrl =
                WebappTestPage.getNonServiceWorkerUrl(mActivityTestRule.getTestServer());
        addAnchorAndClick(otherPageUrl, "_self");
        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), otherPageUrl);

        assertToolbarShowState(activity, false);
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testOpenInChromeFromContextMenuTabbedChrome() throws Exception {
        // Needed to get full context menu.
        FirstRunStatus.setFirstRunFlowComplete(true);
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());

        addAnchor("myTestAnchorId", offOriginUrl(), "_self");

        ContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                null /* activity to check for focus after click */,
                mActivityTestRule.getActivity().getActivityTab(), "myTestAnchorId",
                R.id.contextmenu_open_in_chrome);

        ChromeTabbedActivity tabbedChrome = waitFor(ChromeTabbedActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(tabbedChrome.getActivityTab(), offOriginUrl());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testOpenInChromeFromCustomMenuTabbedChrome() throws Exception {
        WebappActivity activity =
                runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent().putExtra(
                        ShortcutHelper.EXTRA_DISPLAY_MODE, WebDisplayMode.MINIMAL_UI));

        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), activity, R.id.open_in_browser_id);

        ChromeTabbedActivity tabbedChrome = waitFor(ChromeTabbedActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(tabbedChrome.getActivityTab(),
                WebappTestPage.getServiceWorkerUrl(mActivityTestRule.getTestServer()));
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testRegularLinkToExternalApp() throws Exception {
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());

        InterceptNavigationDelegateImpl navigationDelegate =
                mActivityTestRule.getActivity().getActivityTab().getInterceptNavigationDelegate();

        addAnchorAndClick(YOUTUBE_URL, "_self");

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

        addAnchorAndClick(YOUTUBE_URL, "_blank");

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
        ThreadUtils.runOnUiThreadBlocking(
                () -> tabbedChrome.getActivityTab().loadUrl(new LoadUrlParams(offOriginUrl())));
        ChromeTabUtils.waitForTabPageLoaded(tabbedChrome.getActivityTab(), offOriginUrl());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testCloseButtonReturnsToMostRecentInScopeUrl() throws Exception {
        WebappActivity activity = runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());
        Tab tab = activity.getActivityTab();

        String otherInScopeUrl =
                WebappTestPage.getNonServiceWorkerUrl(mActivityTestRule.getTestServer());
        mActivityTestRule.loadUrlInTab(otherInScopeUrl, PageTransition.LINK, tab);
        Assert.assertEquals(otherInScopeUrl, tab.getUrl());

        mActivityTestRule.loadUrlInTab(offOriginUrl(), PageTransition.LINK, tab);
        String mozillaUrl = mActivityTestRule.getTestServer().getURLWithHostName(
                "mozilla.org", "/defaultresponse");
        mActivityTestRule.loadUrlInTab(mozillaUrl, PageTransition.LINK, tab);

        // Toolbar with the close button should be visible.
        assertToolbarShowState(activity, true);

        // Navigate back to in-scope through a close button.
        ThreadUtils.runOnUiThreadBlocking(() -> activity.getToolbarManager()
                .getToolbarLayout().findViewById(R.id.close_button).callOnClick());

        // We should end up on most recent in-scope URL.
        ChromeTabUtils.waitForTabPageLoaded(tab, otherInScopeUrl);
    }

    /**
     * When a CCT is launched as a result of a redirect chain, closing the CCT should return the
     * user to the navigation entry prior to the redirect chain.
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testCloseButtonReturnsToUrlBeforeRedirects() throws Exception {
        Intent launchIntent = mActivityTestRule.createIntent();
        mActivityTestRule.addTwaExtrasToIntent(launchIntent);
        WebappActivity activity = runWebappActivityAndWaitForIdle(launchIntent);

        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        String initialInScopeUrl = WebappTestPage.getServiceWorkerUrl(testServer);
        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), initialInScopeUrl);

        final String redirectingUrl =
                testServer.getURL("/chrome/test/data/android/redirect/js_redirect.html"
                        + "?replace_text="
                        + Base64.encodeToString(
                                  ApiCompatibilityUtils.getBytesUtf8("PARAM_URL"), Base64.URL_SAFE)
                        + ":"
                        + Base64.encodeToString(ApiCompatibilityUtils.getBytesUtf8(offOriginUrl()),
                                  Base64.URL_SAFE));
        addAnchorAndClick(redirectingUrl, "_self");

        CustomTabActivity customTab = waitFor(CustomTabActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(customTab.getActivityTab(), offOriginUrl());

        // Close the CCT.
        assertToolbarShowState(customTab, true);
        ThreadUtils.runOnUiThreadBlocking(()-> customTab.getToolbarManager()
                                                       .getToolbarLayout()
                                                       .findViewById(R.id.close_button)
                                                       .callOnClick());

        // The WebappActivity should be navigated to the page prior to the redirect.
        ChromeTabUtils.waitForTabPageLoaded(activity.getActivityTab(), initialInScopeUrl);
    }

    private WebappActivity runWebappActivityAndWaitForIdle(Intent intent) throws Exception {
        mActivityTestRule.startWebappActivity(intent.putExtra(ShortcutHelper.EXTRA_URL,
                WebappTestPage.getServiceWorkerUrl(mActivityTestRule.getTestServer())));
        mActivityTestRule.waitUntilSplashscreenHides();
        mActivityTestRule.waitUntilIdle();
        return mActivityTestRule.getActivity();
    }

    private void assertToolbarShowState(final ChromeActivity activity, final boolean showState) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals(showState, activity.getActivityTab().canShowBrowserControls());
            }
        });
    }

    private long getDefaultPrimaryColor() {
        return ApiCompatibilityUtils.getColor(
                mActivityTestRule.getActivity().getResources(), R.color.default_primary_color);
    }

    private String offOriginUrl() {
        return mActivityTestRule.getTestServer().getURLWithHostName("foo.com", "/defaultresponse");
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

    private void clickNodeWithId(String id) throws Exception {
        DOMUtils.clickNode(
                mActivityTestRule.getActivity().getActivityTab().getContentViewCore(), id);
    }

    private void addAnchorAndClick(String url, String target) throws Exception {
        addAnchor("testId", url, target);
        clickNodeWithId("testId");
    }

    @SuppressWarnings("unchecked")
    private <T extends ChromeActivity> T waitFor(final Class<T> expectedClass) {
        final Activity[] holder = new Activity[1];
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                holder[0] = ApplicationStatus.getLastTrackedFocusedActivity();
                return holder[0] != null && expectedClass.isAssignableFrom(holder[0].getClass())
                        && ((ChromeActivity) holder[0]).getActivityTab() != null;
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
