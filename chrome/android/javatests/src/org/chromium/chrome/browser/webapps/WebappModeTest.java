// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.preference.PreferenceManager;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.test.MultiActivityTestBase;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.common.ScreenOrientationValues;

import java.lang.ref.WeakReference;
import java.util.List;

/**
 * Tests that WebappActivities are launched correctly.
 *
 * This test seems a little wonky because WebappActivities launched differently, depending on what
 * OS the user is on.  Pre-L, WebappActivities were manually instanced and assigned by the
 * WebappManager.  On L and above, WebappActivities are automatically instanced by Android and the
 * FLAG_ACTIVITY_NEW_DOCUMENT mechanism.  Moreover, we don't have access to the task list pre-L so
 * we have to assume that any non-running WebappActivities are not listed in Android's Overview.
 */
public class WebappModeTest extends MultiActivityTestBase {
    private static final String WEBAPP_1_ID = "webapp_id_1";
    private static final String WEBAPP_1_URL =
            UrlUtils.encodeHtmlDataUri("<html><body bgcolor='#011684'>Webapp 1</body></html>");
    private static final String WEBAPP_1_TITLE = "Web app #1";

    private static final String WEBAPP_2_ID = "webapp_id_2";
    private static final String WEBAPP_2_URL =
            UrlUtils.encodeHtmlDataUri("<html><body bgcolor='#840116'>Webapp 2</body></html>");
    private static final String WEBAPP_2_TITLE = "Web app #2";

    private static final String WEBAPP_ICON = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAACXB"
            + "IWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3wQIFB4cxOfiSQAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdG"
            + "ggR0lNUFeBDhcAAAAMSURBVAjXY2AUawEAALcAnI/TkI8AAAAASUVORK5CYII=";

    private boolean isNumberOfRunningActivitiesCorrect(final int numActivities) throws Exception {
        return CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Context context = getInstrumentation().getTargetContext();
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                        && ApplicationTestUtils.getNumChromeTasks(context) != numActivities) {
                    return false;
                }

                int count = 0;
                List<WeakReference<Activity>> activities = ApplicationStatus.getRunningActivities();
                for (WeakReference<Activity> activity : activities) {
                    if (activity.get() instanceof WebappActivity) count++;
                }
                return count == numActivities;
            }
        });
    }

    private void fireWebappIntent(String id, String url, String title, String icon,
            boolean addMac) throws Exception {
        Intent intent = new Intent();
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setPackage(getInstrumentation().getTargetContext().getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        if (addMac) {
            // Needed for security reasons.  If the MAC is excluded, the URL of the webapp is opened
            // in a browser window, instead.
            String mac = ShortcutHelper.getEncodedMac(getInstrumentation().getTargetContext(), url);
            intent.putExtra(ShortcutHelper.EXTRA_MAC, mac);
        }

        WebappInfo webappInfo = WebappInfo.create(id, url, icon, title, null,
                ScreenOrientationValues.PORTRAIT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING, false);
        webappInfo.setWebappIntentExtras(intent);

        getInstrumentation().getTargetContext().startActivity(intent);
        getInstrumentation().waitForIdleSync();
        ApplicationTestUtils.waitUntilChromeInForeground();
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        // Register the webapps so when the data storage is opened, the test doesn't crash. There is
        // no race condition with the retrival as AsyncTasks are run sequentially on the background
        // thread.
        WebappRegistry.registerWebapp(getInstrumentation().getTargetContext(), WEBAPP_1_ID);
        WebappRegistry.registerWebapp(getInstrumentation().getTargetContext(), WEBAPP_2_ID);
    }

    /**
     * Tests that WebappActivities are started properly.
     */
    @MediumTest
    public void testWebappLaunches() throws Exception {
        final Activity firstActivity =
                startWebappActivity(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON);
        assertTrue(isNumberOfRunningActivitiesCorrect(1));

        // Firing a different Intent should start a new WebappActivity instance.
        fireWebappIntent(WEBAPP_2_ID, WEBAPP_2_URL, WEBAPP_2_TITLE, WEBAPP_ICON, true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return isWebappActivityReady(lastActivity) && lastActivity != firstActivity;
            }
        }));
        assertTrue(isNumberOfRunningActivitiesCorrect(2));

        // Firing the first Intent should bring back the first WebappActivity instance.
        fireWebappIntent(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return isWebappActivityReady(lastActivity) && lastActivity == firstActivity;
            }
        }));
        assertTrue(isNumberOfRunningActivitiesCorrect(2));
    }

    /**
     * Tests that the WebappActivity gets the next available Tab ID instead of 0.
     */
    @MediumTest
    public void testWebappTabIdsProperlyAssigned() throws Exception {
        Context context = getInstrumentation().getTargetContext();
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putInt(TabIdManager.PREF_NEXT_ID, 11684);
        editor.apply();

        final WebappActivity webappActivity =
                startWebappActivity(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON);
        assertTrue(isNumberOfRunningActivitiesCorrect(1));
        assertEquals("Wrong Tab ID was used", 11684, webappActivity.getActivityTab().getId());
    }

    /**
     * Tests that a WebappActivity can be brought forward by calling
     * WebContentsDelegateAndroid.activateContents().
     *
     * Flaky: https://crbug.com/539755
     * @MediumTest
     */
    @DisabledTest
    public void testActivateContents() throws Exception {
        runForegroundingTest(true);
    }

    /**
     * Tests that a WebappActivity can be brought forward by firing an Intent with
     * TabOpenType.BRING_TAB_TO_FRONT.
     *
     * Flaky: https://crbug.com/539755
     * @MediumTest
     */
    @DisabledTest
    public void testBringTabToFront() throws Exception {
        runForegroundingTest(false);
    }

    private void runForegroundingTest(boolean viaActivateContents) throws Exception {
        // Start the WebappActivity.
        final WebappActivity activity =
                startWebappActivity(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON);
        assertTrue(isNumberOfRunningActivitiesCorrect(1));

        // Return home.
        final Context context = getInstrumentation().getTargetContext();
        ApplicationTestUtils.fireHomeScreenIntent(context);
        getInstrumentation().waitForIdleSync();

        if (viaActivateContents) {
            // Bring it back via the Tab.
            activity.getActivityTab().getTabWebContentsDelegateAndroid().activateContents();
        } else {
            // Bring the WebappActivity back via an Intent.
            int webappTabId = activity.getActivityTab().getId();
            Intent intent = Tab.createBringTabToFrontIntent(webappTabId);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
        }

        // When Chrome is back in the foreground, confirm that the original Activity was restored.
        getInstrumentation().waitForIdleSync();
        ApplicationTestUtils.waitUntilChromeInForeground();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return activity == ApplicationStatus.getLastTrackedFocusedActivity()
                        && activity.hasWindowFocus();
            }
        }));
        assertTrue(isNumberOfRunningActivitiesCorrect(1));
    }

    /**
     * Ensure WebappActivities can't be launched without proper security checks.
     */
    @MediumTest
    public void testWebappRequiresValidMac() throws Exception {
        // Try to start a WebappActivity.  Fail because the Intent is insecure.
        fireWebappIntent(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, false);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!lastActivity.findViewById(android.R.id.content).hasWindowFocus()) return false;
                return lastActivity instanceof ChromeTabbedActivity
                        || lastActivity instanceof DocumentActivity;
            }
        }));
        final Activity firstActivity = ApplicationStatus.getLastTrackedFocusedActivity();

        // Firing a correct Intent should start a new WebappActivity instance.
        fireWebappIntent(WEBAPP_2_ID, WEBAPP_2_URL, WEBAPP_2_TITLE, WEBAPP_ICON, true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return isWebappActivityReady(lastActivity) && lastActivity != firstActivity;
            }
        }));
    }

    /**
     * Tests that WebappActivities handle window.open() properly in document mode.
     */
    @DisableInTabbedMode
    @MediumTest
    public void testWebappHandlesWindowOpenInDocumentMode() throws Exception {
        triggerWindowOpenAndWaitForLoad(DocumentActivity.class, ONCLICK_LINK, true);
    }

    /**
     * Tests that WebappActivities handle window.open() properly in tabbed mode.
     */
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_DOCUMENT_MODE)
    @MediumTest
    public void testWebappHandlesWindowOpenInTabbedMode() throws Exception {
        triggerWindowOpenAndWaitForLoad(ChromeTabbedActivity.class, ONCLICK_LINK, true);
    }

    /**
     * Tests that WebappActivities handle suppressed window.open() properly in document mode.
     */
    @DisableInTabbedMode
    @MediumTest
    public void testWebappHandlesSuppressedWindowOpenInDocumentMode() throws Exception {
        triggerWindowOpenAndWaitForLoad(DocumentActivity.class, HREF_NO_REFERRER_LINK, false);
    }

    /**
     * Tests that WebappActivities handle suppressed window.open() properly in tabbed mode.
     */
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_DOCUMENT_MODE)
    @MediumTest
    public void testWebappHandlesSuppressedWindowOpenInTabbedMode() throws Exception {
        triggerWindowOpenAndWaitForLoad(ChromeTabbedActivity.class, HREF_NO_REFERRER_LINK, false);
    }

    private <T extends ChromeActivity> void triggerWindowOpenAndWaitForLoad(
            Class<T> classToWaitFor, String linkHtml, boolean checkContents) throws Exception {
        final WebappActivity webappActivity =
                startWebappActivity(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON);
        assertTrue(isNumberOfRunningActivitiesCorrect(1));

        // Load up the test page.
        assertTrue(CriteriaHelper.pollForCriteria(
                new TabLoadObserver(webappActivity.getActivityTab(), linkHtml)));

        // Do a plain click to make the link open in the main browser via a window.open().
        // If the window is opened successfully, javascript on the first page triggers and changes
        // its URL as a signal for this test.
        Runnable fgTrigger = new Runnable() {
            @Override
            public void run() {
                ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                    @Override
                    public void run() {
                        View view = webappActivity.findViewById(android.R.id.content).getRootView();
                        TouchCommon.singleClickView(view);
                    }
                });
            }
        };
        ChromeActivity secondActivity = ActivityUtils.waitForActivity(
                getInstrumentation(), classToWaitFor, fgTrigger);
        waitForFullLoad(secondActivity, "Page 4");
        if (checkContents) {
            assertEquals("New WebContents was not created",
                    SUCCESS_URL, webappActivity.getActivityTab().getUrl());
        }
        assertNotSame("Wrong Activity in foreground",
                webappActivity, ApplicationStatus.getLastTrackedFocusedActivity());

        // Close the child window to kick the user back to the WebappActivity.
        JavaScriptUtils.executeJavaScript(
                secondActivity.getActivityTab().getWebContents(), "window.close()");
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return webappActivity == ApplicationStatus.getLastTrackedFocusedActivity();
            }
        }));
        ApplicationTestUtils.waitUntilChromeInForeground();
    }

    /**
     * Starts a WebappActivity for the given data and waits for it to be initialized.  We can't use
     * ActivityUtils.waitForActivity() because of the way WebappActivity is instanced on pre-L
     * devices.
     */
    private WebappActivity startWebappActivity(String id, String url, String title, String icon)
            throws Exception {
        fireWebappIntent(id, url, title, icon, true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return isWebappActivityReady(lastActivity);
            }
        }));
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
}
