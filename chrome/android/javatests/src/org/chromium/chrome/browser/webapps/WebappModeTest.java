// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.MultiActivityTestRule;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests that WebappActivities are launched correctly.
 *
 * This test seems a little wonky because WebappActivities launched differently, depending on what
 * OS the user is on.  Pre-L, WebappActivities were manually instanced and assigned by the
 * WebappManager.  On L and above, WebappActivities are automatically instanced by Android and the
 * FLAG_ACTIVITY_NEW_DOCUMENT mechanism.  Moreover, we don't have access to the task list pre-L so
 * we have to assume that any non-running WebappActivities are not listed in Android's Overview.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RetryOnFailure
public class WebappModeTest {
    @Rule
    public MultiActivityTestRule mTestRule = new MultiActivityTestRule();

    private static final String WEBAPP_1_ID = "webapp_id_1";
    private static final String WEBAPP_1_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><title>Web app #1</title><meta name='viewport' "
            + "content='width=device-width initial-scale=0.5, maximum-scale=0.5'></head>"
            + "<body bgcolor='#011684'>Webapp 1</body></html>");
    private static final String WEBAPP_1_TITLE = "Web app #1";

    private static final String WEBAPP_2_ID = "webapp_id_2";
    private static final String WEBAPP_2_URL =
            UrlUtils.encodeHtmlDataUri("<html><body bgcolor='#840116'>Webapp 2</body></html>");
    private static final String WEBAPP_2_TITLE = "Web app #2";

    private static final String WEBAPP_ICON = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAACXB"
            + "IWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3wQIFB4cxOfiSQAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdG"
            + "ggR0lNUFeBDhcAAAAMSURBVAjXY2AUawEAALcAnI/TkI8AAAAASUVORK5CYII=";

    private EmbeddedTestServer mTestServer;

    private Intent createIntent(String id, String url, String title, String icon, boolean addMac) {
        Intent intent = new Intent();
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setPackage(
                InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        if (addMac) {
            // Needed for security reasons.  If the MAC is excluded, the URL of the webapp is opened
            // in a browser window, instead.
            String mac = ShortcutHelper.getEncodedMac(
                    InstrumentationRegistry.getInstrumentation().getTargetContext(), url);
            intent.putExtra(ShortcutHelper.EXTRA_MAC, mac);
        }

        WebappInfo webappInfo = WebappInfo.create(id, url, null, new WebappInfo.Icon(icon), title,
                null, WebDisplayMode.STANDALONE, ScreenOrientationValues.PORTRAIT,
                ShortcutSource.UNKNOWN, ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING, false);
        webappInfo.setWebappIntentExtras(intent);

        return intent;
    }

    private void fireWebappIntent(String id, String url, String title, String icon,
            boolean addMac) {
        Intent intent = createIntent(id, url, title, icon, addMac);

        InstrumentationRegistry.getInstrumentation().getTargetContext().startActivity(intent);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        ApplicationTestUtils.waitUntilChromeInForeground();
    }

    @Before
    public void setUp() throws Exception {
        WebappRegistry.refreshSharedPrefsForTesting();

        // Register the webapps so when the data storage is opened, the test doesn't crash. There is
        // no race condition with the retrieval as AsyncTasks are run sequentially on the background
        // thread.
        WebappRegistry.getInstance().register(
                WEBAPP_1_ID, new WebappRegistry.FetchWebappDataStorageCallback() {
                    @Override
                    public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                        storage.updateFromShortcutIntent(createIntent(
                                WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, true));
                    }
                });
        WebappRegistry.getInstance().register(
                WEBAPP_2_ID, new WebappRegistry.FetchWebappDataStorageCallback() {
                    @Override
                    public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                        storage.updateFromShortcutIntent(createIntent(
                                WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, true));
                    }
                });

        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Tests that WebappActivities are started properly.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testWebappLaunches() {
        final WebappActivity firstActivity =
                startWebappActivity(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON);
        final int firstTabId = firstActivity.getActivityTab().getId();

        // Firing a different Intent should start a new WebappActivity instance.
        fireWebappIntent(WEBAPP_2_ID, WEBAPP_2_URL, WEBAPP_2_TITLE, WEBAPP_ICON, true);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!isWebappActivityReady(lastActivity)) return false;

                WebappActivity lastWebappActivity = (WebappActivity) lastActivity;
                return lastWebappActivity.getActivityTab().getId() != firstTabId;
            }
        });

        // Firing the first Intent should bring back the first WebappActivity instance, or at least
        // a WebappActivity with the same tab if the other one was killed by Android mid-test.
        fireWebappIntent(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, true);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!isWebappActivityReady(lastActivity)) return false;

                WebappActivity lastWebappActivity = (WebappActivity) lastActivity;
                return lastWebappActivity.getActivityTab().getId() == firstTabId;
            }
        });
    }

    /**
     * Tests that the WebappActivity gets the next available Tab ID instead of 0.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testWebappTabIdsProperlyAssigned() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putInt(TabIdManager.PREF_NEXT_ID, 11684);
        editor.apply();

        final WebappActivity webappActivity =
                startWebappActivity(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON);
        Assert.assertEquals(
                "Wrong Tab ID was used", 11684, webappActivity.getActivityTab().getId());
    }

    /**
     * Tests that a WebappActivity can be brought forward by firing an Intent with
     * TabOpenType.BRING_TAB_TO_FRONT.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testBringTabToFront() throws Exception {
        // Start the WebappActivity.
        final WebappActivity firstActivity =
                startWebappActivity(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON);
        final int webappTabId = firstActivity.getActivityTab().getId();

        // Return home.
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        ApplicationTestUtils.fireHomeScreenIntent(context);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Bring the WebappActivity back via an Intent.
        Intent intent = Tab.createBringTabToFrontIntent(webappTabId);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);

        // When Chrome is back in the foreground, confirm that the correct Activity was restored.
        // Because of Android killing Activities willy-nilly, it may not be the same Activity, but
        // it should have the same Tab ID.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        ApplicationTestUtils.waitUntilChromeInForeground();
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!isWebappActivityReady(lastActivity)) return false;

                WebappActivity webappActivity = (WebappActivity) lastActivity;
                return webappActivity.getActivityTab().getId() == webappTabId;
            }
        });
    }

    /**
     * Ensure WebappActivities can't be launched without proper security checks.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testWebappRequiresValidMac() {
        // Try to start a WebappActivity.  Fail because the Intent is insecure.
        fireWebappIntent(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, false);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return lastActivity instanceof ChromeTabbedActivity;
            }
        });
        ChromeActivity chromeActivity =
                (ChromeActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        mTestRule.waitForFullLoad(chromeActivity, WEBAPP_1_TITLE);

        // Firing a correct Intent should start a WebappActivity instance instead of the browser.
        fireWebappIntent(WEBAPP_2_ID, WEBAPP_2_URL, WEBAPP_2_TITLE, WEBAPP_ICON, true);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return isWebappActivityReady(ApplicationStatus.getLastTrackedFocusedActivity());
            }
        });
    }

    /**
     * Tests that WebappActivities handle window.open() properly in tabbed mode.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testWebappHandlesWindowOpenInTabbedMode() throws Exception {
        triggerWindowOpenAndWaitForLoad(ChromeTabbedActivity.class, getOnClickLinkUrl(), true);
    }

    /**
     * Tests that WebappActivities handle suppressed window.open() properly in tabbed mode.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testWebappHandlesSuppressedWindowOpenInTabbedMode() throws Exception {
        triggerWindowOpenAndWaitForLoad(
                ChromeTabbedActivity.class, getHrefNoReferrerLinkUrl(), false);
    }

    private <T extends ChromeActivity> void triggerWindowOpenAndWaitForLoad(
            Class<T> classToWaitFor, String linkHtml, boolean checkContents) throws Exception {
        final WebappActivity firstActivity =
                startWebappActivity(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON);
        final int firstWebappId = firstActivity.getActivityTab().getId();

        // Load up the test page.
        new TabLoadObserver(firstActivity.getActivityTab()).fullyLoadUrl(linkHtml);

        // Do a plain click to make the link open in the main browser via a window.open().
        // If the window is opened successfully, javascript on the first page triggers and changes
        // its URL as a signal for this test.
        Runnable fgTrigger = new Runnable() {
            @Override
            public void run() {
                ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                    @Override
                    public void run() {
                        View view = firstActivity.findViewById(android.R.id.content).getRootView();
                        TouchCommon.singleClickView(view);
                    }
                });
            }
        };
        ChromeActivity secondActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), classToWaitFor, fgTrigger);
        mTestRule.waitForFullLoad(secondActivity, "The Google");
        if (checkContents) {
            Assert.assertEquals("New WebContents was not created", "SUCCESS",
                    firstActivity.getActivityTab().getTitle());
        }
        Assert.assertNotSame("Wrong Activity in foreground", firstActivity,
                ApplicationStatus.getLastTrackedFocusedActivity());

        // Close the child window to kick the user back to the WebappActivity.
        JavaScriptUtils.executeJavaScript(
                secondActivity.getActivityTab().getWebContents(), "window.close()");
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!isWebappActivityReady(lastActivity)) return false;

                WebappActivity webappActivity = (WebappActivity) lastActivity;
                return webappActivity.getActivityTab().getId() == firstWebappId;
            }
        });
        ApplicationTestUtils.waitUntilChromeInForeground();
    }

    /**
     * Starts a WebappActivity for the given data and waits for it to be initialized.  We can't use
     * ActivityUtils.waitForActivity() because of the way WebappActivity is instanced on pre-L
     * devices.
     */
    private WebappActivity startWebappActivity(String id, String url, String title, String icon) {
        fireWebappIntent(id, url, title, icon, true);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return isWebappActivityReady(lastActivity);
            }
        });
        return (WebappActivity) ApplicationStatus.getLastTrackedFocusedActivity();
    }

    /** Returns true when the last Activity is a WebappActivity and is ready for testing .*/
    private boolean isWebappActivityReady(Activity lastActivity) {
        if (!(lastActivity instanceof WebappActivity)) return false;

        WebappActivity webappActivity = (WebappActivity) lastActivity;
        if (webappActivity.getActivityTab() == null) return false;

        View rootView = webappActivity.findViewById(android.R.id.content);
        if (!rootView.hasWindowFocus()) return false;

        return true;
    }

    /** Defines one gigantic link spanning the whole page that creates a new
     *  window with chrome/test/data/android/google.html. Disallowing a referrer from being
     *  sent triggers another codepath.
     */
    private String getHrefNoReferrerLinkUrl() {
        return UrlUtils.encodeHtmlDataUri("<html>"
                + "  <head>"
                + "    <title>href no referrer link page</title>"
                + "    <meta name='viewport'"
                + "        content='width=device-width initial-scale=0.5, maximum-scale=0.5'>"
                + "    <style>"
                + "      body {margin: 0em;} div {width: 100%; height: 100%; background: #011684;}"
                + "    </style>"
                + "  </head>"
                + "  <body>"
                + "    <a href='" + mTestServer.getURL("/chrome/test/data/android/google.html")
                + "' target='_blank' rel='noreferrer'><div></div></a>"
                + "  </body>");
    }

    /** Returns a URL where clicking the body triggers a window.open() call to open
     * chrome/test/data/android/google.html. */
    private String getOnClickLinkUrl() {
        return UrlUtils.encodeHtmlDataUri("<html>"
                + "  <head>"
                + "    <title>window.open page</title>"
                + "    <meta name='viewport'"
                + "        content='width=device-width initial-scale=0.5, maximum-scale=0.5'>"
                + "    <style>"
                + "      body {margin: 0em;} div {width: 100%; height: 100%; background: #011684;}"
                + "    </style>"
                + "    <script>"
                + "      function openNewWindow() {"
                + "        var site = window.open('"
                + mTestServer.getURL("/chrome/test/data/android/google.html") + "');"
                + "        document.title = site ? 'SUCCESS' : 'FAILURE';"
                + "      }"
                + "    </script>"
                + "  </head>"
                + "  <body id='body'>"
                + "    <div onclick='openNewWindow()'></div>"
                + "  </body>"
                + "</html>");
    }
}
