// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.view.ContextMenu;
import android.view.View;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.document.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.test.MultiActivityTestBase;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.LoadUrlParams;

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

    private static class TestTabObserver extends EmptyTabObserver {
        private ContextMenu mContextMenu;

        @Override
        public void onContextMenuShown(Tab tab, ContextMenu menu) {
            mContextMenu = menu;
        }
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
                ChromeApplication.getDocumentTabModelSelector().getModel(false).getCount());
        return tabIds;
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
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
        return launchUrlViaRunnable(incognito, runnable, expectedTitle, false);
    }

    /** Starts a DocumentActivity using {@ref ChromeLauncherActivity.launchDocumentInstance().} */
    protected int launchViaLaunchDocumentInstance(final boolean incognito, final String url,
            final String expectedTitle) throws Exception {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                AsyncTabCreationParams asyncParams =
                        new AsyncTabCreationParams(new LoadUrlParams(url));
                ChromeLauncherActivity.launchDocumentInstance(null, incognito, asyncParams);
            }
        };
        return launchUrlViaRunnable(incognito, runnable, expectedTitle, false);
    }

    /** Starts a DocumentActivity in background using
     * {@ref ChromeLauncherActivity.launchDocumentInstance().} */
    protected int launchViaLaunchDocumentInstanceInBackground(final boolean incognito,
            final String url, final String expectedTitle) throws Exception {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                AsyncTabCreationParams asyncParams =
                        new AsyncTabCreationParams(new LoadUrlParams(url));
                asyncParams.setDocumentLaunchMode(ChromeLauncherActivity.LAUNCH_MODE_AFFILIATED);
                ChromeLauncherActivity.launchDocumentInstance(null, incognito, asyncParams);
            }
        };
        return launchUrlViaRunnable(incognito, runnable, expectedTitle, true);
    }

    /**
     * Launches a DocumentActivity via the given Runnable.
     * Ideally this would use Observers, but we can't know that the DocumentTabModelSelector has
     * been created, and don't want to influence the test by accidentally creating it during the
     * test suite runs.
     * @return ID of the Tab that was launched.
     */
    protected int launchUrlViaRunnable(final boolean incognito, final Runnable runnable,
            final String expectedTitle, final boolean launchedInBackground) throws Exception {
        final int tabCount =
                ChromeApplication.isDocumentTabModelSelectorInitializedForTests()
                ? ChromeApplication.getDocumentTabModelSelector().getModel(incognito)
                        .getCount() : 0;
        final int tabId =
                ChromeApplication.isDocumentTabModelSelectorInitializedForTests()
                ? ChromeApplication.getDocumentTabModelSelector().getCurrentTabId()
                : Tab.INVALID_TAB_ID;

        // Wait for the Activity to start up.
        final DocumentActivity newActivity = ActivityUtils.waitForActivity(
                getInstrumentation(),
                incognito ? IncognitoDocumentActivity.class : DocumentActivity.class, runnable);

        assertTrue(ChromeApplication.isDocumentTabModelSelectorInitializedForTests());
        ApplicationTestUtils.waitUntilChromeInForeground();

        // Wait until the selector is ready and the Tabs have been added to the DocumentTabModel.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (!ChromeApplication.isDocumentTabModelSelectorInitializedForTests()) {
                    return false;
                }

                DocumentTabModelSelector selector =
                        ChromeApplication.getDocumentTabModelSelector();
                if (selector.isIncognitoSelected() != incognito) return false;
                if (selector.getModel(incognito).getCount() != (tabCount + 1)) return false;
                if (selector.getCurrentTabId() == tabId) return false;
                return true;
            }
        }));

        if (launchedInBackground) {
            ChromeTabUtils.waitForTabPageLoaded(newActivity.getActivityTab(), (String) null);
        } else {
            waitForFullLoad(newActivity, expectedTitle);
        }
        return ChromeApplication.getDocumentTabModelSelector().getCurrentTabId();
    }

    /**
     * Long presses at the center of the page, selects "Open In New Tab" option from the menu.
     */
    protected void openLinkInBackgroundTab() throws Exception {
        // We expect tab to open in the background, i.e. tab index / id should
        // stay the same.
        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        final TabModel tabModel = selector.getModel(false);
        final int expectedTabCount = tabModel.getCount() + 1;
        final int expectedTabIndex = tabModel.index();
        final int expectedTabId = selector.getCurrentTabId();
        final DocumentActivity expectedActivity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();

        openLinkInNewTabViaContextMenu(false, false);

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
     *
     * Note that incognito context menus don't display the "Open in incognito tab" menu item.
     * Instead, selecting "Open in new tab" when in incognito mode will open a new incognito
     * tab in the background.  Specifically:
     *     Regular tab + "Open in new tab" -> Opens new regular tab in the background
     *     Regular tab + "Open in incognito tab" -> Opens new incognito tab in the foreground
     *     Incognito tab + "Open in new tab" -> Opens new incognito tab in the background
     *     Incognito tab + "Open in incognito tab" -> Can't happen.
     *
     * @param selectOpenInIncognitoTab If true, selects "Open in incognito tab".
     *                                 If false, selects "Open in new tab".
     * @param waitForIncognito If true, waits for an IncognitoDocumentActivity to start.
     *                         If false, waits for a DocumentActivity to start.
     */
    protected void openLinkInNewTabViaContextMenu(final boolean selectOpenInIncognitoTab,
            final boolean waitForIncognito) throws Exception {
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
        TouchCommon.longPressView(view);
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return observer.mContextMenu != null;
            }
        }));
        activity.getActivityTab().removeObserver(observer);

        // Select the "open in new tab" option.
        DocumentActivity newActivity = ActivityUtils.waitForActivity(getInstrumentation(),
                waitForIncognito ? IncognitoDocumentActivity.class : DocumentActivity.class,
                new Runnable() {
                    @Override
                    public void run() {
                        if (selectOpenInIncognitoTab) {
                            assertTrue(observer.mContextMenu.performIdentifierAction(
                                    R.id.contextmenu_open_in_incognito_tab, 0));
                        } else {
                            assertTrue(observer.mContextMenu.performIdentifierAction(
                                    R.id.contextmenu_open_in_new_tab, 0));
                        }
                    }
                }
        );
        if (selectOpenInIncognitoTab) {
            waitForFullLoad(newActivity, "Page 4");
        } else {
            ChromeTabUtils.waitForTabPageLoaded(newActivity.getActivityTab(), (String) null);
        }
    }

    /**
     * Switches to the specified tab and waits until its activity is brought to the foreground.
     */
    protected void switchToTab(final DocumentTab tab) throws Exception {
        final TabModel tabModel =
                ChromeApplication.getDocumentTabModelSelector().getCurrentModel();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                // http://crbug.com/509866: TabModelUtils#setIndex() sometimes fails.
                // So we need to call it repeatedly.
                getInstrumentation().runOnMainSync(new Runnable() {
                    @Override
                    public void run() {
                        TabModelUtils.setIndex(tabModel,
                                TabModelUtils.getTabIndexById(tabModel, tab.getId()));
                    }
                });

                return ApplicationStatus.getStateForActivity(tab.getActivity())
                        == ActivityState.RESUMED;
            }
        }));
    }

    /**
     * Starts ChromeTabbedActivity via intent with the specified URL but
     * doesn't wait for the URL to load.
     * @return The started activity.
     */
    protected ChromeTabbedActivity startTabbedActivity(final String url) {
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent();
                intent.setAction(Intent.ACTION_VIEW);
                intent.setClassName(mContext, ChromeTabbedActivity.class.getName());
                intent.setData(Uri.parse(url));
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
                mContext.startActivity(intent);
            }
        };
        return ActivityUtils.waitForActivity(getInstrumentation(), ChromeTabbedActivity.class,
                runnable);
    }
}
