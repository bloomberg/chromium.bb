// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;
import android.view.ContextMenu;
import android.view.View;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeMobileApplication;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.test.MultiActivityTestBase;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.chrome.test.util.browser.tabmodel.document.MockStorageDelegate;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.Callable;

/**
 * Used for testing the interactions of the DocumentTabModel with Android's ActivityManager.  This
 * test suite actually fires Intents to start different DocumentActivities.
 *
 * Note that this is different instrumenting one DocumentActivity in particular, which should be
 * tested using the DocumentActivityTestBase class.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
@DisableInTabbedMode
public class DocumentModeTestBase extends MultiActivityTestBase {
    protected static final String TAG = "cr.document";

    protected static final String URL_1 = createTestUrl(1);
    protected static final String URL_2 = createTestUrl(2);
    protected static final String URL_3 = createTestUrl(3);
    protected static final String URL_4 = createTestUrl(4);

    // Defines one gigantic link spanning the whole page that creates a new window.
    protected static final String HREF_LINK = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "  <head>"
            + "    <title>href link page</title>"
            + "    <meta name='viewport'"
            + "        content='width=device-width initial-scale=0.5, maximum-scale=0.5'>"
            + "    <style>"
            + "      body {margin: 0em;} div {width: 100%; height: 100%; background: #011684;}"
            + "    </style>"
            + "  </head>"
            + "  <body>"
            + "    <a href='" + URL_4 + "' target='_blank'><div></div></a>"
            + "  </body>"
            + "</html>");

    // Clicking the body triggers a window.open() call.
    protected static final String SUCCESS_URL = UrlUtils.encodeHtmlDataUri("opened!");
    protected static final String ONCLICK_LINK = UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "  <head>"
            + "    <title>window.open page</title>"
            + "    <meta name='viewport'"
            + "        content='width=device-width initial-scale=0.5, maximum-scale=0.5'>"
            + "    <style>"
            + "      body {margin: 0em;} div {width: 100%; height: 100%; background: #011684;}"
            + "    </style>"
            + "    <script>"
            + "      function openNewWindow() {"
            + "        if (window.open('" + URL_4 + "')) location.href = '" + SUCCESS_URL + "';"
            + "      }"
            + "    </script>"
            + "  </head>"
            + "  <body id='body'>"
            + "    <div onclick='openNewWindow()'></div></a>"
            + "  </body>"
            + "</html>");

    private static final float FLOAT_EPSILON = 0.001f;

    private static class TestTabObserver extends EmptyTabObserver {
        private ContextMenu mContextMenu;

        @Override
        public void onContextMenuShown(Tab tab, ContextMenu menu) {
            mContextMenu = menu;
        }
    }

    protected Context mContext;
    protected MockStorageDelegate mStorageDelegate;

    protected void launchHomeIntent(Context context) throws Exception {
        Intent startMain = new Intent(Intent.ACTION_MAIN);
        startMain.addCategory(Intent.CATEGORY_HOME);
        startMain.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(startMain);
        MultiActivityTestBase.waitUntilChromeInBackground();
    }

    protected void launchMainIntent(Context context) throws Exception {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setPackage(context.getPackageName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
        MultiActivityTestBase.waitUntilChromeInForeground();
    }

    protected void fireViewIntent(Context context, Uri data) throws Exception {
        Intent intent = new Intent(Intent.ACTION_VIEW, data);
        intent.setClassName(context.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
        MultiActivityTestBase.waitUntilChromeInForeground();
    }

    /**
     * Launches three tabs via Intents with ACTION_VIEW.
     */
    protected int[] launchThreeTabs() throws Exception {
        int[] tabIds = new int[3];
        tabIds[0] = launchViaViewIntent(false, URL_1, "Page 1");
        tabIds[1] = launchViaViewIntent(false, URL_2, "Page 2");
        tabIds[2] = launchViaViewIntent(false, URL_3, "Page 3");
        assertFalse(tabIds[0] == tabIds[1]);
        assertFalse(tabIds[0] == tabIds[2]);
        assertFalse(tabIds[1] == tabIds[2]);
        assertEquals(3,
                ChromeMobileApplication.getDocumentTabModelSelector().getModel(false).getCount());
        return tabIds;
    }

    @Override
    public void setUp() throws Exception {
        mContext = getInstrumentation().getTargetContext();

        mStorageDelegate = new MockStorageDelegate(mContext.getCacheDir());
        DocumentTabModelSelector.setStorageDelegateForTests(mStorageDelegate);

        super.setUp();
    }

    @Override
    public void tearDown() throws Exception {
        mStorageDelegate.ensureDirectoryDestroyed();
        super.tearDown();
    }

    /** Starts a DocumentActivity by using firing a VIEW Intent. */
    protected int launchViaViewIntent(final boolean incognito, final String url,
            final String expectedTitle) throws Exception {
        // Fire the Intent and wait until Chrome is in the foreground.
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                Runnable runnable = new Runnable() {
                    @Override
                    public void run() {
                        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
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
        return launchUrlViaRunnable(incognito, runnable, expectedTitle);
    }

    /** Starts a DocumentActivity using {@ref ChromeLauncherActivity.launchDocumentInstance().} */
    protected int launchViaLaunchDocumentInstance(final boolean incognito, final String url,
            final String expectedTitle) throws Exception {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                ChromeLauncherActivity.launchDocumentInstance(null, incognito,
                        ChromeLauncherActivity.LAUNCH_MODE_FOREGROUND, url,
                        DocumentMetricIds.STARTED_BY_UNKNOWN, PageTransition.LINK, false, null);
            }
        };
        return launchUrlViaRunnable(incognito, runnable, expectedTitle);
    }

    /**
     * Launches a DocumentActivity via the given Runnable.
     * Ideally this would use Observers, but we can't know that the DocumentTabModelSelector has
     * been created, and don't want to influence the test by accidentally creating it during the
     * test suite runs.
     * @return ID of the Tab that was launched.
     */
    protected int launchUrlViaRunnable(final boolean incognito, final Runnable runnable,
            final String expectedTitle) throws Exception {
        final int tabCount =
                ChromeMobileApplication.isDocumentTabModelSelectorInitializedForTests()
                ? ChromeMobileApplication.getDocumentTabModelSelector().getModel(incognito)
                        .getCount() : 0;
        final int tabId =
                ChromeMobileApplication.isDocumentTabModelSelectorInitializedForTests()
                ? ChromeMobileApplication.getDocumentTabModelSelector().getCurrentTabId()
                : Tab.INVALID_TAB_ID;

        // Wait for the Activity to start up.
        final DocumentActivity newActivity = (DocumentActivity) ActivityUtils.waitForActivity(
                getInstrumentation(),
                incognito ? IncognitoDocumentActivity.class : DocumentActivity.class, runnable);

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

        waitForFullLoad(newActivity, expectedTitle);
        return ChromeMobileApplication.getDocumentTabModelSelector().getCurrentTabId();
    }

    /**
     * Approximates when a DocumentActivity is fully ready and loaded, which is hard to gauge
     * because Android's Activity transition animations are not monitorable.
     */
    protected void waitForFullLoad(final DocumentActivity activity, final String expectedTitle)
            throws Exception {
        assertWaitForPageScaleFactorMatch(activity, 0.5f);
        final Tab tab = activity.getActivityTab();
        assert tab != null;

        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (!tab.isLoadingAndRenderingDone()) return false;
                if (!TextUtils.equals(expectedTitle, tab.getTitle())) return false;
                return true;
            }
        }));
    }

    /**
     * Proper use of this function requires waiting for a page scale factor that isn't 1.0f because
     * the default seems to be 1.0f.
     * TODO(dfalcantara): Combine this one and ChromeActivityTestCaseBase's (crbug.com/498973)
     */
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
     * Long presses at the center of the page, selects "Open In New Tab" option from the menu.
     */
    protected void openLinkInBackgroundTab() throws Exception {
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

    /**
     * Long presses at the center of the page and opens a new Tab via a link's context menu.
     */
    protected void openLinkInNewTabViaContextMenu(final boolean incognito) throws Exception {
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

        // Select the "open in new tab" option.
        DocumentActivity newActivity = ActivityUtils.waitForActivity(getInstrumentation(),
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
        waitForFullLoad(newActivity, "Page 4");
    }

    private static final String createTestUrl(int index) {
        String[] colors = {"#000000", "#ff0000", "#00ff00", "#0000ff", "#ffff00"};
        return UrlUtils.encodeHtmlDataUri(
            "<html>"
            + "  <head>"
            + "    <title>Page " + index + "</title>"
            + "    <meta name='viewport' content='width=device-width "
            + "        initial-scale=0.5 maximum-scale=0.5'>"
            + "  </head>"
            + "  <body style='margin: 0em; background: " + colors[index] + ";'></body>"
            + "</html>");
    }
}
