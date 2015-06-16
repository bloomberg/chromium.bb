// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.view.ContextMenu;
import android.view.View;

import com.google.android.apps.chrome.R;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
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
    protected static final String URL_1 = "data:text/html;charset=utf-8,Page%201";
    protected static final String URL_2 = "data:text/html;charset=utf-8,Page%202";
    protected static final String URL_3 = "data:text/html;charset=utf-8,Page%203";
    protected static final String URL_4 = "data:text/html;charset=utf-8,Page%204";

    protected static final String LANDING_PAGE = UrlUtils.encodeHtmlDataUri(
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
    protected static final float LANDING_PAGE_SCALE = 1.0f;

    // Defines one gigantic link spanning the whole page that creates a new window.
    protected static final String HREF_LINK = UrlUtils.encodeHtmlDataUri(
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
    protected static final String ONCLICK_LINK = UrlUtils.encodeHtmlDataUri(
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
    private static final float FLOAT_EPSILON = 0.001f;

    private static class TestTabObserver extends EmptyTabObserver {
        private ContextMenu mContextMenu;

        @Override
        public void onContextMenuShown(Tab tab, ContextMenu menu) {
            mContextMenu = menu;
        }
    }

    protected Context mContext;

    protected static void launchMainIntent(Context context) throws Exception {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setPackage(context.getPackageName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
        MultiActivityTestBase.waitUntilChromeInForeground();
    }

    protected static void fireViewIntent(Context context, Uri data) throws Exception {
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

    /** Starts a DocumentActivity by using firing a VIEW Intent. */
    protected int launchViaViewIntent(final boolean incognito, final String url) throws Exception {
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
    protected int launchViaLaunchDocumentInstance(
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
    protected int launchUrlViaRunnable(final boolean incognito, final Runnable runnable)
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
    protected void launchTestPageDocument(String link) throws Exception {
        launchViaLaunchDocumentInstance(false, link);
        final DocumentActivity activity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        assertWaitForPageScaleFactorMatch(activity, HTML_SCALE);
    }

    // TODO(dfalcantara): Combine this one and ChromeActivityTestCaseBase's.
    protected void assertWaitForPageScaleFactorMatch(
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
}
