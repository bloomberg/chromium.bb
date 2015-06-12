// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.RunningServiceInfo;
import android.app.Instrumentation;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.provider.Browser;
import android.test.MoreAsserts;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.ContextMenu;
import android.view.View;

import com.google.android.apps.chrome.R;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.BookmarkUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeMobileApplication;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel.InitializationObserver;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelImpl;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.OffTheRecordDocumentTabModel;
import org.chromium.chrome.test.MultiActivityTestBase;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.content.app.SandboxedProcessService;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

import java.lang.ref.WeakReference;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests the interactions of the DocumentTabModel with Android's ActivityManager.  This test suite
 * actually fires Intents to start different DocumentActivities.
 *
 * Note that this is different instrumenting one DocumentActivity in particular, which should be
 * tested using the DocumentActivityTestBase class.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
@DisableInTabbedMode
public class DocumentModeTest extends MultiActivityTestBase {
    private static final float FLOAT_EPSILON = 0.001f;

    private static final int ACTIVITY_START_TIMEOUT = 1000;
    private static final String URL_1 = "data:text/html;charset=utf-8,Page%201";
    private static final String URL_2 = "data:text/html;charset=utf-8,Page%202";
    private static final String URL_3 = "data:text/html;charset=utf-8,Page%203";
    private static final String URL_4 = "data:text/html;charset=utf-8,Page%204";

    private static final String LANDING_PAGE = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "  <head>"
            + "    <meta name='viewport' content='width=device-width "
            + "        initial-scale=1.0, maximum-scale=1.0'>"
            + "    <style>"
            + "        body {margin: 0em;} div {width: 100%; height: 100%; background: #ff00ff;}"
            + "    </style>"
            + "  </head>"
            + "  <body>Second page</body>"
            + "</html>");
    private static final float LANDING_PAGE_SCALE = 1.0f;

    // Defines one gigantic link spanning the whole page that creates a new window.
    private static final String HREF_LINK = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "  <head>"
            + "    <meta name='viewport'"
            + "        content='width=device-width initial-scale=0.5, maximum-scale=0.5'>"
            + "    <style>"
            + "      body {margin: 0em;} div {width: 100%; height: 100%; background: #011684;}"
            + "    </style>"
            + "  </head>"
            + "  <body>"
            + "    <a href='" + LANDING_PAGE + "' target='_blank'><div></div></a>"
            + "  </body>"
            + "</html>");

    // Clicking the body triggers a window.open() call.
    private static final String ONCLICK_LINK = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "  <head>"
            + "    <meta name='viewport'"
            + "        content='width=device-width initial-scale=0.5, maximum-scale=0.5'>"
            + "    <style>"
            + "      body {margin: 0em;} div {width: 100%; height: 100%; background: #011684;}"
            + "    </style>"
            + "  </head>"
            + "  <body id='body'>"
            + "    <div onclick='window.open(\"" + LANDING_PAGE + "\")'></div></a>"
            + "  </body>"
            + "</html>");
    private static final float HTML_SCALE = 0.5f;

    private boolean mInitializationCompleted;

    private Context mContext;
    private String mUrl;
    private String mReferrer;
    private TabModelSelectorTabObserver mObserver;

    private static class TestTabObserver extends EmptyTabObserver {
        private ContextMenu mContextMenu;

        @Override
        public void onContextMenuShown(Tab tab, ContextMenu menu) {
            mContextMenu = menu;
        }
    }

    private static void launchMainIntent(Context context) throws Exception {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setPackage(context.getPackageName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
        MultiActivityTestBase.waitUntilChromeInForeground();
    }

    private static void fireViewIntent(Context context, Uri data) throws Exception {
        Intent intent = new Intent(Intent.ACTION_VIEW, data);
        intent.setClassName(context.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
        MultiActivityTestBase.waitUntilChromeInForeground();
    }

    /**
     * Launches three tabs via Intents with ACTION_VIEW.
     */
    private int[] launchThreeTabs() throws Exception {
        int[] tabIds = new int[3];
        tabIds[0] = launchViaViewIntent(false, URL_1);
        tabIds[1] = launchViaViewIntent(false, URL_2);
        tabIds[2] = launchViaViewIntent(false, URL_3);
        assertFalse(tabIds[0] == tabIds[1]);
        assertFalse(tabIds[0] == tabIds[2]);
        assertFalse(tabIds[1] == tabIds[2]);
        assertEquals(3,
                ChromeMobileApplication.getDocumentTabModelSelector().getModel(false).getCount());
        return tabIds;
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getTargetContext();
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();
        if (mObserver != null) {
            mObserver.destroy();
            mObserver = null;
        }
    }

    /**
     * Confirm that you can't start ChromeTabbedActivity while the user is running in Document mode.
     */
    @MediumTest
    public void testDontStartTabbedActivityInDocumentMode() throws Exception {
        launchThreeTabs();

        // Try launching a ChromeTabbedActivity.
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent();
                intent.setAction(Intent.ACTION_VIEW);
                intent.setClassName(mContext, ChromeTabbedActivity.class.getName());
                intent.setData(Uri.parse(URL_1));
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
                mContext.startActivity(intent);
            }
        };
        ActivityUtils.waitForActivity(getInstrumentation(), ChromeTabbedActivity.class, runnable);

        // ApplicationStatus should note that the ChromeTabbedActivity isn't running anymore.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                List<WeakReference<Activity>> activities = ApplicationStatus.getRunningActivities();
                for (WeakReference<Activity> activity : activities) {
                    if (activity.get() instanceof ChromeTabbedActivity) return false;
                }
                return true;
            }
        }));
    }

    /**
     * Confirm that firing an Intent without a properly formatted document://ID?url scheme causes
     * the DocumentActivity to finish itself and hopefully not flat crash (though that'd be better
     * than letting the user live in both tabbed and document mode simultaneously crbug.com/445136).
     */
    @MediumTest
    public void testFireInvalidIntent() throws Exception {
        launchThreeTabs();

        final DocumentTabModelSelector selector =
                ChromeMobileApplication.getDocumentTabModelSelector();
        final Activity lastTrackedActivity = ApplicationStatus.getLastTrackedFocusedActivity();

        // Send the user home, then fire an Intent with invalid data.
        MultiActivityTestBase.launchHomescreenIntent(mContext);
        Intent intent = new Intent(lastTrackedActivity.getIntent());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.setData(Uri.parse("toteslegitscheme://"));
        mContext.startActivity(intent);

        // A DocumentActivity gets started, but it should immediately call finishAndRemoveTask()
        // because of the broken Intent.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
                return activity != lastTrackedActivity;
            }
        }));

        // We shouldn't record that a new Tab exists.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return selector.getCurrentModel().getCount() == 3
                        && selector.getTotalTabCount() == 3;
            }
        }));
    }

    /**
     * Confirm that firing an Intent for a document that has an ID for an already existing Tab kills
     * the original.
     */
    @MediumTest
    public void testDuplicateTabIDsKillsOldActivities() throws Exception {
        launchThreeTabs();

        final DocumentTabModelSelector selector =
                ChromeMobileApplication.getDocumentTabModelSelector();
        final int lastTabId = selector.getCurrentTabId();
        final Activity lastTrackedActivity = ApplicationStatus.getLastTrackedFocusedActivity();

        // Send the user home, then fire an Intent with an old Tab ID and a new URL.
        MultiActivityTestBase.launchHomescreenIntent(mContext);
        Intent intent = new Intent(lastTrackedActivity.getIntent());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.setData(Uri.parse("document://" + lastTabId + "?" + URL_4));
        mContext.startActivity(intent);

        // Funnily enough, Android doesn't differentiate between URIs with different queries when
        // refocusing Activities based on the Intent data.  This means we can't do a check to see
        // that the new Activity appears with URL_4 -- we just get a new instance of URL_3.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!(activity instanceof ChromeActivity)) return false;

                ChromeActivity chromeActivity = (ChromeActivity) activity;
                Tab tab = chromeActivity.getActivityTab();
                return tab != null && lastTrackedActivity != activity
                        && !selector.isIncognitoSelected()
                        && lastTabId == selector.getCurrentTabId();
            }
        }));

        // Although we get a new DocumentActivity, the old one with the same tab ID gets killed.
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return selector.getCurrentModel().getCount() == 3
                    && selector.getTotalTabCount() == 3;
            }
        }));
    }

    /**
     * Confirm that firing a View Intent with a null URL acts like a Main Intent.
     */
    @MediumTest
    public void testRelaunchLatestTabWithInvalidViewIntent() throws Exception {
        launchThreeTabs();

        final DocumentTabModelSelector selector =
                ChromeMobileApplication.getDocumentTabModelSelector();
        final int lastTabId = selector.getCurrentTabId();

        final Activity lastTrackedActivity = ApplicationStatus.getLastTrackedFocusedActivity();

        // Send Chrome to the background, then bring it back.
        MultiActivityTestBase.launchHomescreenIntent(mContext);
        fireViewIntent(mContext, null);

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return lastTrackedActivity == ApplicationStatus.getLastTrackedFocusedActivity()
                        && !selector.isIncognitoSelected()
                        && lastTabId == selector.getCurrentTabId();
            }
        }));

        assertEquals(3, selector.getCurrentModel().getCount());
        assertEquals(3, selector.getTotalTabCount());
    }

    /**
     * Confirm that clicking the Chrome icon (e.g. firing an Intent with ACTION_MAIN) brings back
     * the last viewed Tab.
     */
    @MediumTest
    public void testRelaunchLatestTab() throws Exception {
        launchThreeTabs();

        final DocumentTabModelSelector selector =
                ChromeMobileApplication.getDocumentTabModelSelector();
        final int lastTabId = selector.getCurrentTabId();

        // Send Chrome to the background, then bring it back.
        MultiActivityTestBase.launchHomescreenIntent(mContext);
        launchMainIntent(mContext);

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !selector.isIncognitoSelected() && lastTabId == selector.getCurrentTabId();
            }
        }));

        assertEquals(3, selector.getCurrentModel().getCount());
        assertEquals(3, selector.getTotalTabCount());
    }

    @MediumTest
    public void testReferrerExtra() throws Exception {
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                DocumentActivity.class.getName(), null, false);
        launchMainIntent(mContext);

        // Wait for tab model to become initialized.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ChromeMobileApplication.isDocumentTabModelSelectorInitializedForTests();
            }
        }));

        DocumentTabModelSelector selector = ChromeMobileApplication.getDocumentTabModelSelector();
        mObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                mUrl = params.getUrl();
                if (params.getReferrer() != null) {
                    mReferrer = params.getReferrer().getUrl();
                }
            }
        };

        // Wait for document activity from the main intent to launch before firing the next
        // intent.
        DocumentActivity doc = (DocumentActivity) monitor.waitForActivityWithTimeout(
                ACTIVITY_START_TIMEOUT);
        assertNotNull("DocumentActivity did not start", doc);

        mUrl = null;
        mReferrer = null;

        // Fire the Intent with EXTRA_REFERRER.
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(URL_1));
        intent.setClassName(mContext.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(URL_2));
        IntentHandler.addTrustedIntentExtras(intent, mContext);
        mContext.startActivity(intent);

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return URL_1.equals(mUrl);
            }
        }));

        assertEquals(URL_2, mReferrer);
    }

    @MediumTest
    public void testReferrerExtraAndroidApp() throws Exception {
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                DocumentActivity.class.getName(), null, false);
        launchMainIntent(mContext);

        // Wait for tab model to become initialized.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ChromeMobileApplication.isDocumentTabModelSelectorInitializedForTests();
            }
        }));

        DocumentTabModelSelector selector = ChromeMobileApplication.getDocumentTabModelSelector();
        mObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                mUrl = params.getUrl();
                if (params.getReferrer() != null) {
                    mReferrer = params.getReferrer().getUrl();
                }
            }
        };

        // Wait for document activity from the main intent to launch before firing the next
        // intent.
        DocumentActivity doc = (DocumentActivity) monitor.waitForActivityWithTimeout(
                ACTIVITY_START_TIMEOUT);
        assertNotNull("DocumentActivity did not start", doc);

        mUrl = null;
        mReferrer = null;

        // Fire the Intent with EXTRA_REFERRER using android-app scheme.
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(URL_1));
        intent.setClassName(mContext.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        final String androidAppReferrer = "android-app://com.imdb.mobile/http/www.imdb.com";
        intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(androidAppReferrer));
        mContext.startActivity(intent);

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return URL_1.equals(mUrl);
            }
        }));

        assertEquals(androidAppReferrer, mReferrer);
    }

    @MediumTest
    public void testReferrerExtraNotAndroidApp() throws Exception {
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                DocumentActivity.class.getName(), null, false);
        launchMainIntent(mContext);

        // Wait for tab model to become initialized.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ChromeMobileApplication.isDocumentTabModelSelectorInitializedForTests();
            }
        }));

        DocumentTabModelSelector selector = ChromeMobileApplication.getDocumentTabModelSelector();
        mObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                mUrl = params.getUrl();
                if (params.getReferrer() != null) {
                    mReferrer = params.getReferrer().getUrl();
                }
            }
        };

        // Wait for document activity from the main intent to launch before firing the next
        // intent.
        DocumentActivity doc = (DocumentActivity) monitor.waitForActivityWithTimeout(
                ACTIVITY_START_TIMEOUT);
        assertNotNull("DocumentActivity did not start", doc);

        mUrl = null;
        mReferrer = null;

        // Fire the third-party Intent with EXTRA_REFERRER using a regular URL.
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(URL_1));
        intent.setClassName(mContext.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        final String nonAppExtra = "http://www.imdb.com";
        intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(nonAppExtra));
        mContext.startActivity(intent);

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return URL_1.equals(mUrl);
            }
        }));

        // Check that referrer is not carried over
        assertNull(mReferrer);
    }

    @MediumTest
    public void testReferrerExtraFromExternalIntent() throws Exception {
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                DocumentActivity.class.getName(), null, false);
        launchMainIntent(mContext);

        // Wait for tab model to become initialized.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ChromeMobileApplication.isDocumentTabModelSelectorInitializedForTests();
            }
        }));

        DocumentTabModelSelector selector = ChromeMobileApplication.getDocumentTabModelSelector();
        mObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                mUrl = params.getUrl();
                if (params.getReferrer() != null) {
                    mReferrer = params.getReferrer().getUrl();
                }
            }
        };

        // Wait for document activity from the main intent to launch before firing the next
        // intent.
        DocumentActivity doc = (DocumentActivity) monitor.waitForActivityWithTimeout(
                ACTIVITY_START_TIMEOUT);
        assertNotNull("DocumentActivity did not start", doc);

        mUrl = null;
        mReferrer = null;

        // Simulate a link click inside the application that goes through external navigation
        // handler and then goes back to Chrome.
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(URL_1));
        intent.setClassName(mContext.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
        IntentHandler.setPendingReferrer(intent, "http://www.google.com");
        mContext.startActivity(intent);

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return URL_1.equals(mUrl);
            }
        }));

        // Check that referrer is not carried over
        assertEquals("http://www.google.com", mReferrer);
    }

    /**
     * Confirm that setting the index brings the correct tab forward.
     */
    @MediumTest
    public void testSetIndex() throws Exception {
        int[] tabIds = launchThreeTabs();

        final DocumentTabModelSelector selector =
                ChromeMobileApplication.getDocumentTabModelSelector();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(selector.getCurrentModel(), 0);
            }
        });

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !selector.isIncognitoSelected() && selector.getCurrentModel().index() == 0;
            }
        }));

        assertEquals(3, selector.getCurrentModel().getCount());
        assertEquals(3, selector.getTotalTabCount());
        assertEquals(tabIds[0], selector.getCurrentTab().getId());
    }

    /** Check that Intents that request reusing Tabs are honored. */
    @MediumTest
    public void testReuseIntent() throws Exception {
        // Create a tab, then send the user back to the Home screen.
        int tabId = launchViaViewIntent(false, URL_1);
        assertTrue(ChromeMobileApplication.isDocumentTabModelSelectorInitializedForTests());
        final DocumentTabModelSelector selector =
                ChromeMobileApplication.getDocumentTabModelSelector();
        assertEquals(1, selector.getModel(false).getCount());
        launchHomescreenIntent(mContext);

        // Fire an Intent to reuse the same tab as before.
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(URL_1));
                intent.setClass(mContext, ChromeLauncherActivity.class);
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                intent.putExtra(BookmarkUtils.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
                mContext.startActivity(intent);
            }
        };
        ActivityUtils.waitForActivity(getInstrumentation(), ChromeLauncherActivity.class, runnable);
        waitUntilChromeInForeground();
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return lastActivity instanceof DocumentActivity;
            }
        }));
        assertEquals(tabId, selector.getCurrentTabId());
        assertFalse(selector.isIncognitoSelected());

        // Create another tab.
        final int secondTabId = launchViaViewIntent(false, URL_2);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return selector.getModel(false).getCount() == 2
                        && selector.getCurrentTabId() == secondTabId;
            }
        }));
    }

    /**
     * Tests both ways of launching Incognito tabs: via an Intent, and via
     * {@ref ChromeLauncherActivity#launchDocumentInstance()}.
     */
    @MediumTest
    public void testIncognitoLaunches() throws Exception {
        assertFalse(ChromeMobileApplication.isDocumentTabModelSelectorInitializedForTests());

        // Make sure that an untrusted Intent can't launch an IncognitoDocumentActivity.
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                DocumentActivity.class.getName(), null, false);
        assertFalse(ChromeMobileApplication.isDocumentTabModelSelectorInitializedForTests());
        assertEquals(0, ApplicationStatus.getRunningActivities().size());
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(URL_1));
                intent.setClass(mContext, ChromeLauncherActivity.class);
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
                mContext.startActivity(intent);
            }
        };
        ActivityUtils.waitForActivity(getInstrumentation(), ChromeLauncherActivity.class, runnable);
        DocumentActivity doc = (DocumentActivity) monitor.waitForActivityWithTimeout(1000);
        assertNull(doc);

        // Create an Incognito tab via an Intent extra.
        final int firstId = launchViaViewIntent(true, URL_2);
        assertTrue(ChromeMobileApplication.isDocumentTabModelSelectorInitializedForTests());
        final DocumentTabModelSelector selector =
                ChromeMobileApplication.getDocumentTabModelSelector();
        final TabModel incognitoModel = selector.getModel(true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return firstId == selector.getCurrentTabId() && selector.getTotalTabCount() == 1;
            }
        }));
        assertEquals(incognitoModel, selector.getCurrentModel());

        // Launch via ChromeLauncherActivity.launchInstance().
        final int secondId = launchViaLaunchDocumentInstance(true, URL_3);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return secondId == selector.getCurrentTabId() && selector.getTotalTabCount() == 2;
            }
        }));
        assertTrue(selector.isIncognitoSelected());
        assertEquals(incognitoModel, selector.getCurrentModel());
        assertEquals(secondId, TabModelUtils.getCurrentTabId(incognitoModel));
    }

    /**
     * Confirm that the incognito tabs and TabModel are destroyed when the "close all" notification
     * Intent is fired.
     */
    @MediumTest
    public void testIncognitoNotificationClosesTabs() throws Exception {
        final int regularId = launchViaLaunchDocumentInstance(false, URL_1);
        final DocumentTabModelSelector selector =
                ChromeMobileApplication.getDocumentTabModelSelector();
        assertFalse(selector.isIncognitoSelected());

        final int incognitoId = launchViaLaunchDocumentInstance(true, URL_2);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return selector.isIncognitoSelected() && selector.getCurrentTabId() == incognitoId;
            }
        }));
        assertEquals(0, selector.getCurrentModel().index());
        assertEquals(1, selector.getCurrentModel().getCount());

        PendingIntent closeAllIntent =
                ChromeLauncherActivity.getRemoveAllIncognitoTabsIntent(mContext);
        closeAllIntent.send();

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return selector.getCurrentTabId() == regularId;
            }
        }));
        OffTheRecordDocumentTabModel tabModel =
                (OffTheRecordDocumentTabModel) selector.getModel(true);
        assertFalse(selector.isIncognitoSelected());
        assertFalse(tabModel.isDocumentTabModelImplCreated());
    }

    /**
     * Tests if a tab is covered by its child activity.
     */
    @MediumTest
    public void testCoveredByChildActivity() throws Exception {
        final int tabId = launchViaLaunchDocumentInstance(false, URL_1);
        final DocumentTabModel model =
                ChromeMobileApplication.getDocumentTabModelSelector().getModelForTabId(tabId);
        final Tab tab = model.getTabAt(0);
        assertTrue(tab instanceof DocumentTab);
        final DocumentTab documentTab = (DocumentTab) tab;

        // We need to wait until the UI for document tab is initialized. So we create the
        // InitializationObserver and set its satisfied criteria the same as
        // DocumentActivity.mTabInitializationObserver.
        InitializationObserver observer = new InitializationObserver(model) {
                @Override
                public boolean isSatisfied(int currentState) {
                    return currentState >= DocumentTabModelImpl.STATE_LOAD_TAB_STATE_BG_END
                            || model.isTabStateReady(tabId);
                }

                @Override
                public boolean isCanceled() {
                    return false;
                }

                @Override
                public void runImmediately() {
                    // This observer is created before DocumentActivity.mTabInitializationObserver.
                    // Postpone setting mInitializationCompleted afterwards.
                    ThreadUtils.postOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            mInitializationCompleted = true;
                        }
                    });
                }
        };
        observer.runWhenReady();

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mInitializationCompleted;
            }
        }));

        assertFalse(documentTab.isCoveredByChildActivity());
        assertFalse(model.isCoveredByChildActivity(tabId));

        documentTab.setCoveredByChildActivity(true);
        assertTrue(documentTab.isCoveredByChildActivity());
        assertTrue(model.isCoveredByChildActivity(tabId));

        documentTab.setCoveredByChildActivity(false);
        assertFalse(documentTab.isCoveredByChildActivity());
        assertFalse(model.isCoveredByChildActivity(tabId));
    }

    /**
     * Tests that Incognito tabs are opened in the foreground.
     */
    @MediumTest
    public void testIncognitoOpensInForegroundViaLinkContextMenu() throws Exception {
        launchTestPageDocument(HREF_LINK);

        // Save the current tab info.
        final DocumentActivity regularActivity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        final DocumentTabModelSelector selector =
                ChromeMobileApplication.getDocumentTabModelSelector();
        final TabModel regularTabModel = selector.getModel(false);
        final int regularTabId = selector.getCurrentTabId();

        // Open a link in incognito via the context menu.
        openLinkInNewTabViaContextMenu(true);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (1 != regularTabModel.getCount()) return false;
                if (regularTabId == selector.getCurrentTabId()) return false;
                if (!selector.isIncognitoSelected()) return false;
                return true;
            }
        }));
        assertEquals(0, selector.getCurrentModel().index());
        MoreAsserts.assertNotEqual(
                regularActivity, ApplicationStatus.getLastTrackedFocusedActivity());

        // Re-open the other tab.
        TabModelUtils.setIndex(regularTabModel, 0);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !selector.isIncognitoSelected()
                        && selector.getCurrentTabId() == regularTabId;
            }
        }));

        // Try to open a new Incognito Tab in the background using the TabModelSelector directly.
        // Should open it in the foreground, instead.
        final DocumentActivity currentActivity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                LoadUrlParams params = new LoadUrlParams(URL_1);
                currentActivity.getTabModelSelector().openNewTab(
                        params, TabModel.TabLaunchType.FROM_LONGPRESS_BACKGROUND, null, true);
            }
        };
        ActivityUtils.waitForActivity(
                getInstrumentation(), IncognitoDocumentActivity.class, runnable);
        MoreAsserts.assertNotEqual(
                currentActivity, ApplicationStatus.getLastTrackedFocusedActivity());
    }

    /**
     * Tests that tabs opened via window.open() load properly.
     */
    @MediumTest
    public void testWindowOpen() throws Exception {
        launchTestPageDocument(ONCLICK_LINK);

        final DocumentActivity firstActivity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();

        // Save the current tab ID.
        final DocumentTabModelSelector selector =
                ChromeMobileApplication.getDocumentTabModelSelector();
        final TabModel tabModel = selector.getModel(false);
        final int firstTabId = selector.getCurrentTabId();
        final int firstTabIndex = tabModel.index();

        // Do a plain click to make the link open in a new foreground Document via a window.open().
        Runnable fgTrigger = new Runnable() {
            @Override
            public void run() {
                try {
                    DOMUtils.clickNode(null, firstActivity.getCurrentContentViewCore(), "body");
                } catch (Exception e) {

                }
            }
        };

        // The WebContents gets paused because of the window.open.  Checking that the page scale
        // factor is set correctly both checks that the page loaded and that the WebContents was
        // when the new Activity starts.
        ChromeActivity secondActivity = (ChromeActivity) ActivityUtils.waitForActivity(
                getInstrumentation(), DocumentActivity.class, fgTrigger);
        assertWaitForPageScaleFactorMatch(secondActivity, LANDING_PAGE_SCALE);

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (2 != tabModel.getCount()) return false;
                if (firstTabIndex == tabModel.index()) return false;
                if (firstTabId == selector.getCurrentTabId()) return false;
                return true;
            }
        }));
        MoreAsserts.assertNotEqual(
                firstActivity, ApplicationStatus.getLastTrackedFocusedActivity());
    }

    /**
     * Tests that tab ID is properly set when tabs change.
     */
    @MediumTest
    public void testLastTabIdUpdates() throws Exception {
        launchTestPageDocument(HREF_LINK);

        final DocumentActivity firstActivity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();

        // Save the current tab ID.
        final DocumentTabModelSelector selector =
                ChromeMobileApplication.getDocumentTabModelSelector();
        final TabModel tabModel = selector.getModel(false);
        final int firstTabId = selector.getCurrentTabId();
        final int firstTabIndex = tabModel.index();

        openLinkInBackgroundTab();

        // Do a plain click to make the link open in a new foreground Document.
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
        ActivityUtils.waitForActivity(getInstrumentation(), DocumentActivity.class, fgTrigger);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (3 != tabModel.getCount()) return false;
                if (firstTabIndex == tabModel.index()) return false;
                if (firstTabId == selector.getCurrentTabId()) return false;
                return true;
            }
        }));
        MoreAsserts.assertNotEqual(
                firstActivity, ApplicationStatus.getLastTrackedFocusedActivity());
    }

    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    @MediumTest
    public void testNewTabLoadLowEnd() throws Exception {
        launchTestPageDocument(HREF_LINK);

        final CallbackHelper tabCreatedCallback = new CallbackHelper();
        final CallbackHelper tabLoadStartedCallback = new CallbackHelper();

        final DocumentTabModelSelector selector =
                ChromeMobileApplication.getDocumentTabModelSelector();
        selector.addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onNewTabCreated(final Tab newTab) {
                selector.removeObserver(this);
                tabCreatedCallback.notifyCalled();

                assertFalse(newTab.getWebContents().isLoadingToDifferentDocument());

                newTab.addObserver(new EmptyTabObserver() {
                    @Override
                    public void onPageLoadStarted(Tab tab, String url) {
                        newTab.removeObserver(this);
                        tabLoadStartedCallback.notifyCalled();
                    }
                });
            }
        });

        openLinkInBackgroundTab();

        // Tab should be created, but shouldn't start loading until we switch to it.
        assertEquals(1, tabCreatedCallback.getCallCount());
        assertEquals(0, tabLoadStartedCallback.getCallCount());

        TabModelUtils.setIndex(selector.getCurrentModel(), 1);
        tabLoadStartedCallback.waitForCallback(0);
    }

    /**
     * Tests that "Open in new tab" command doesn't create renderer per tab
     * on low end devices.
     */
    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    @MediumTest
    public void testNewTabRenderersLowEnd() throws Exception {
        launchTestPageDocument(HREF_LINK);

        // Ignore any side effects that the first background tab might produce.
        openLinkInBackgroundTab();

        int rendererCountBefore = countRenderers();

        final int newTabCount = 5;
        for (int i = 0; i != newTabCount; ++i) {
            openLinkInBackgroundTab();
        }

        int rendererCountAfter = countRenderers();

        assertEquals(rendererCountBefore, rendererCountAfter);
    }

    /** Starts a DocumentActivity by using firing a VIEW Intent. */
    private int launchViaViewIntent(final boolean incognito, final String url) throws Exception {
        // Fire the Intent and wait until Chrome is in the foreground.
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                Runnable runnable = new Runnable() {
                    @Override
                    public void run() {
                        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(URL_1));
                        intent.setClass(mContext, ChromeLauncherActivity.class);
                        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        if (incognito) {
                            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
                            IntentHandler.startActivityForTrustedIntent(intent, mContext);
                        } else {
                            mContext.startActivity(intent);
                        }
                    }
                };
                ActivityUtils.waitForActivity(getInstrumentation(),
                        (incognito ? IncognitoDocumentActivity.class : DocumentActivity.class),
                        runnable);
            }
        };
        return launchUrlViaRunnable(incognito, runnable);
    }

    /** Starts a DocumentActivity using {@ref ChromeLauncherActivity.launchDocumentInstance().} */
    private int launchViaLaunchDocumentInstance(
            final boolean incognito, final String url) throws Exception {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                ChromeLauncherActivity.launchDocumentInstance(null, incognito,
                        ChromeLauncherActivity.LAUNCH_MODE_FOREGROUND, url,
                        DocumentMetricIds.STARTED_BY_UNKNOWN, PageTransition.LINK, false, null);
            }
        };
        return launchUrlViaRunnable(incognito, runnable);
    }

    /**
     * Launches a DocumentActivity via the given Runnable.
     * Ideally this would use Observers, but we can't know that the DocumentTabModelSelector has
     * been created, and don't want to influence the test by accidentally creating it during the
     * test suite runs.
     * @return ID of the Tab that was launched.
     */
    private int launchUrlViaRunnable(final boolean incognito, final Runnable runnable)
            throws Exception {
        final int tabCount =
                ChromeMobileApplication.isDocumentTabModelSelectorInitializedForTests()
                ? ChromeMobileApplication.getDocumentTabModelSelector().getModel(incognito)
                        .getCount() : 0;
        final int tabId =
                ChromeMobileApplication.isDocumentTabModelSelectorInitializedForTests()
                ? ChromeMobileApplication.getDocumentTabModelSelector().getCurrentTabId()
                : Tab.INVALID_TAB_ID;

        runnable.run();
        assertTrue(ChromeMobileApplication.isDocumentTabModelSelectorInitializedForTests());
        MultiActivityTestBase.waitUntilChromeInForeground();

        // Wait until the selector is ready and the Tabs have been added to the DocumentTabModel.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (!ChromeMobileApplication.isDocumentTabModelSelectorInitializedForTests()) {
                    return false;
                }

                DocumentTabModelSelector selector =
                        ChromeMobileApplication.getDocumentTabModelSelector();
                if (selector.isIncognitoSelected() != incognito) return false;
                if (selector.getModel(incognito).getCount() != (tabCount + 1)) return false;
                if (selector.getCurrentTabId() == tabId) return false;
                return true;
            }
        }));

        return ChromeMobileApplication.getDocumentTabModelSelector().getCurrentTabId();
    }

    /**
     * Launches DocumentActivity with a page that can be used to create a second page.
     */
    private void launchTestPageDocument(String link) throws Exception {
        launchViaLaunchDocumentInstance(false, link);
        final DocumentActivity activity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        assertWaitForPageScaleFactorMatch(activity, HTML_SCALE);
    }

    // TODO(dfalcantara): Combine this one and ChromeActivityTestCaseBase's.
    private void assertWaitForPageScaleFactorMatch(
            final ChromeActivity activity, final float expectedScale) throws Exception {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (activity.getCurrentContentViewCore() == null) return false;

                return Math.abs(activity.getCurrentContentViewCore().getScale() - expectedScale)
                        < FLOAT_EPSILON;
            }
        }));
    }

    /**
     * Long presses at the center of the page, selects "Open In New Tab" option
     * from the menu.
     */
    private void openLinkInBackgroundTab() throws Exception {
        // We expect tab to open in the background, i.e. tab index / id should
        // stay the same.
        final DocumentTabModelSelector selector =
                ChromeMobileApplication.getDocumentTabModelSelector();
        final TabModel tabModel = selector.getModel(false);
        final int expectedTabCount = tabModel.getCount() + 1;
        final int expectedTabIndex = tabModel.index();
        final int expectedTabId = selector.getCurrentTabId();
        final DocumentActivity expectedActivity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();

        openLinkInNewTabViaContextMenu(false);

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (expectedTabCount != tabModel.getCount()) return false;
                if (expectedTabIndex != tabModel.index()) return false;
                if (expectedTabId != selector.getCurrentTabId()) return false;
                return true;
            }
        }));

        assertEquals(expectedActivity, ApplicationStatus.getLastTrackedFocusedActivity());
    }


    private void openLinkInNewTabViaContextMenu(final boolean incognito) throws Exception {
        // Long press the center of the page, which should bring up the context menu.
        final TestTabObserver observer = new TestTabObserver();
        final DocumentActivity activity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        activity.getActivityTab().addObserver(observer);
        assertNull(observer.mContextMenu);
        final View view = ThreadUtils.runOnUiThreadBlocking(new Callable<View>() {
            @Override
            public View call() throws Exception {
                return activity.findViewById(android.R.id.content).getRootView();
            }
        });
        TouchCommon.longPressView(view, view.getWidth() / 2, view.getHeight() / 2);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return observer.mContextMenu != null;
            }
        }));
        activity.getActivityTab().removeObserver(observer);

        // Select the "open in new tab" option to open a tab in the background.
        ChromeActivity newActivity = ActivityUtils.waitForActivity(getInstrumentation(),
                incognito ? IncognitoDocumentActivity.class : DocumentActivity.class,
                new Runnable() {
                    @Override
                    public void run() {
                        if (incognito) {
                            assertTrue(observer.mContextMenu.performIdentifierAction(
                                    R.id.contextmenu_open_in_incognito_tab, 0));
                        } else {
                            assertTrue(observer.mContextMenu.performIdentifierAction(
                                    R.id.contextmenu_open_in_new_tab, 0));
                        }
                    }
                }
        );
        assertWaitForPageScaleFactorMatch(newActivity, LANDING_PAGE_SCALE);
    }

    /**
     * Returns the number of currently running renderer services.
     */
    private int countRenderers() {
        ActivityManager activityManager = (ActivityManager) mContext.getSystemService(
                Context.ACTIVITY_SERVICE);

        int rendererCount = 0;
        List<RunningServiceInfo> serviceInfos = activityManager.getRunningServices(
                Integer.MAX_VALUE);
        for (RunningServiceInfo serviceInfo : serviceInfos) {
            if (serviceInfo.service.getClassName().startsWith(
                    SandboxedProcessService.class.getName())) {
                rendererCount++;
            }
        }

        return rendererCount;
    }
}
