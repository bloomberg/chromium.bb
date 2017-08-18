// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browseractions;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.app.PendingIntent;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.customtabs.browseractions.BrowserActionsIntent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.util.Pair;
import android.util.SparseArray;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.browseractions.BrowserActionsContextMenuHelper.BrowserActionsTestDelegate;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuItem;
import org.chromium.chrome.browser.contextmenu.ContextMenuItem;
import org.chromium.chrome.browser.contextmenu.ShareContextMenuItem;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.List;

/**
 * Instrumentation tests for context menu of a {@link BrowserActionActivity}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BrowserActionActivityTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/test.html";
    private static final String TEST_PAGE_3 = "/chrome/test/data/android/simple.html";
    private static final String CUSTOM_ITEM_TITLE = "Custom item";

    private final CallbackHelper mOnBrowserActionsMenuShownCallback = new CallbackHelper();
    private final CallbackHelper mOnFinishNativeInitializationCallback = new CallbackHelper();
    private final CallbackHelper mOnOpenTabInBackgroundStartCallback = new CallbackHelper();

    private BrowserActionsContextMenuItemDelegate mMenuItemDelegate;
    private SparseArray<PendingIntent> mCustomActions;
    private List<Pair<Integer, List<ContextMenuItem>>> mItems;
    private ProgressDialog mProgressDialog;
    private TestDelegate mTestDelegate;
    private EmbeddedTestServer mTestServer;
    private String mTestPage;
    private String mTestPage2;
    private String mTestPage3;
    private PendingIntent mCustomPendingItent;
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private class TestDelegate implements BrowserActionsTestDelegate {
        @Override
        public void onBrowserActionsMenuShown() {
            mOnBrowserActionsMenuShownCallback.notifyCalled();
        }

        @Override
        public void onFinishNativeInitialization() {
            mOnFinishNativeInitializationCallback.notifyCalled();
        }

        @Override
        public void onOpenTabInBackgroundStart() {
            mOnOpenTabInBackgroundStartCallback.notifyCalled();
        }

        @Override
        public void initialize(BrowserActionsContextMenuItemDelegate menuItemDelegate,
                SparseArray<PendingIntent> customActions,
                List<Pair<Integer, List<ContextMenuItem>>> items,
                ProgressDialog progressDialog) {
            mMenuItemDelegate = menuItemDelegate;
            mCustomActions = customActions;
            mItems = items;
            mProgressDialog = progressDialog;
        }
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FirstRunStatus.setFirstRunFlowComplete(true);
            }
        });
        mTestDelegate = new TestDelegate();
        Context appContext = InstrumentationRegistry.getInstrumentation()
                                     .getTargetContext()
                                     .getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
        mTestPage3 = mTestServer.getURL(TEST_PAGE_3);
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FirstRunStatus.setFirstRunFlowComplete(false);
            }
        });
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    public void testMenuShownCorrectly() throws Exception {
        startBrowserActionActivity(mTestPage);

        // Menu should be shown before native finish loading.
        mOnBrowserActionsMenuShownCallback.waitForCallback(0);
        Assert.assertEquals(0, mOnFinishNativeInitializationCallback.getCallCount());
        Assert.assertEquals(0, mOnOpenTabInBackgroundStartCallback.getCallCount());

        // Let the initialization completes.
        mOnFinishNativeInitializationCallback.waitForCallback(0);
        Assert.assertEquals(1, mOnBrowserActionsMenuShownCallback.getCallCount());
        Assert.assertEquals(1, mOnFinishNativeInitializationCallback.getCallCount());

        // Check menu populated correctly.
        List<Pair<Integer, List<ContextMenuItem>>> menus = mItems;
        Assert.assertEquals(1, menus.size());
        List<ContextMenuItem> items = menus.get(0).second;
        Assert.assertEquals(6, items.size());
        for (int i = 0; i < 4; i++) {
            Assert.assertTrue(items.get(i) instanceof ChromeContextMenuItem);
        }
        Assert.assertTrue(items.get(4) instanceof ShareContextMenuItem);
        Assert.assertTrue(items.get(5) instanceof BrowserActionsCustomContextMenuItem);
        Assert.assertEquals(mCustomPendingItent,
                mCustomActions.get(
                        BrowserActionsContextMenuHelper.CUSTOM_BROWSER_ACTIONS_ID_GROUP.get(0)));
    }

    @Test
    @SmallTest
    public void testOpenTabInBackgroundAfterInitialization() throws Exception {
        final BrowserActionActivity activity = startBrowserActionActivity(mTestPage);
        mOnBrowserActionsMenuShownCallback.waitForCallback(0);
        // Open a tab in background before initialization finishes.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                activity.getHelperForTesting().onItemSelected(
                        R.id.browser_actions_open_in_background);
            }
        });

        // A ProgressDialog should be displayed and tab opening should be pending until
        // initialization finishes.
        Assert.assertTrue(mProgressDialog.isShowing());
        Assert.assertEquals(0, mOnOpenTabInBackgroundStartCallback.getCallCount());
        mOnFinishNativeInitializationCallback.waitForCallback(0);
        mOnOpenTabInBackgroundStartCallback.waitForCallback(0);
        Assert.assertFalse(mProgressDialog.isShowing());
        Assert.assertEquals(1, mOnOpenTabInBackgroundStartCallback.getCallCount());
    }

    @Test
    @SmallTest
    public void testOpenSingleTabInBackgroundWhenChromeAvailable() throws Exception {
        // Start ChromeTabbedActivity first.
        mActivityTestRule.startMainActivityWithURL(mTestPage);

        // Load Browser Actions menu completely.
        final BrowserActionActivity activity = startBrowserActionActivity(mTestPage2);
        mOnBrowserActionsMenuShownCallback.waitForCallback(0);
        mOnFinishNativeInitializationCallback.waitForCallback(0);
        Assert.assertEquals(1, mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        // No notification should be shown.
        Assert.assertFalse(mMenuItemDelegate.hasBrowserActionsNotification());

        // Open a tab in the background.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                activity.getHelperForTesting().onItemSelected(
                        R.id.browser_actions_open_in_background);
            }
        });

        // Notification for single tab should be shown.
        Assert.assertTrue(mMenuItemDelegate.hasBrowserActionsNotification());
        // Tabs should always be added at the end of the model.
        Assert.assertEquals(2, mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        Assert.assertEquals(mTestPage,
                mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getUrl());
        Assert.assertEquals(mTestPage2,
                mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(1).getUrl());
        int prevTabId = mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId();
        int newTabId = mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(1).getId();
        // TODO(ltian): overwrite delegate prevent creating notifcation for test.
        Intent notificationIntent = mMenuItemDelegate.getNotificationIntent();
        notificationIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Force ChromeTabbedActivity dismissed to make sure it calls onStop then calls onStart next
        // time it is started by an Intent.
        Intent customTabIntent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getInstrumentation().getTargetContext(), mTestPage);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);

        // The Intent of the Browser Actions notification should not toggle overview mode and should
        mActivityTestRule.startActivityCompletely(notificationIntent);
        Assert.assertFalse(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        Assert.assertNotEquals(prevTabId, mActivityTestRule.getActivity().getActivityTab().getId());
        Assert.assertEquals(newTabId, mActivityTestRule.getActivity().getActivityTab().getId());
    }

    @Test
    @SmallTest
    public void testOpenMulitpleTabInBackgroundWhenChromeAvailable() throws Exception {
        // Start ChromeTabbedActivity first.
        mActivityTestRule.startMainActivityWithURL(mTestPage);

        // Open two tabs in the background.
        final BrowserActionActivity activity1 = startBrowserActionActivity(mTestPage2);
        mOnBrowserActionsMenuShownCallback.waitForCallback(0);
        mOnFinishNativeInitializationCallback.waitForCallback(0);
        Assert.assertEquals(1, mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                activity1.getHelperForTesting().onItemSelected(
                        R.id.browser_actions_open_in_background);
            }
        });

        final BrowserActionActivity activity2 = startBrowserActionActivity(mTestPage3, 1);
        mOnBrowserActionsMenuShownCallback.waitForCallback(1);
        mOnFinishNativeInitializationCallback.waitForCallback(1);
        Assert.assertEquals(2, mActivityTestRule.getActivity().getCurrentTabModel().getCount());

        // Notification for multiple tabs should be shown.
        Assert.assertTrue(mMenuItemDelegate.hasBrowserActionsNotification());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                activity2.getHelperForTesting().onItemSelected(
                        R.id.browser_actions_open_in_background);
            }
        });

        // Tabs should always be added at the end of the model.
        Assert.assertEquals(3, mActivityTestRule.getActivity().getCurrentTabModel().getCount());
        Assert.assertEquals(mTestPage,
                mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getUrl());
        Assert.assertEquals(mTestPage2,
                mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(1).getUrl());
        Assert.assertEquals(mTestPage3,
                mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(2).getUrl());
        Intent notificationIntent = mMenuItemDelegate.getNotificationIntent();
        notificationIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // Force ChromeTabbedActivity dismissed to make sure it calls onStop then calls onStart next
        // time it is started by an Intent.
        Intent customTabIntent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getInstrumentation().getTargetContext(), mTestPage);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(customTabIntent);

        // The Intent of the Browser Actions notification should toggle overview mode.
        mActivityTestRule.startActivityCompletely(notificationIntent);
        if (DeviceFormFactor.isTablet()) {
            Assert.assertFalse(
                    mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        } else {
            Assert.assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());
        }
    }

    private BrowserActionActivity startBrowserActionActivity(String url) throws Exception {
        return startBrowserActionActivity(url, 0);
    }

    private BrowserActionActivity startBrowserActionActivity(String url, int expectedCallCount)
            throws Exception {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        ActivityMonitor browserActionMonitor =
                new ActivityMonitor(BrowserActionActivity.class.getName(), null, false);
        instrumentation.addMonitor(browserActionMonitor);

        // The BrowserActionActivity shouldn't have started yet.
        Assert.assertEquals(expectedCallCount, mOnBrowserActionsMenuShownCallback.getCallCount());
        Assert.assertEquals(
                expectedCallCount, mOnFinishNativeInitializationCallback.getCallCount());
        Assert.assertEquals(expectedCallCount, mOnOpenTabInBackgroundStartCallback.getCallCount());

        // Fire an Intent to start the BrowserActionActivity.
        sendBrowserActionIntent(instrumentation, url);

        Activity activity = instrumentation.waitForMonitorWithTimeout(
                browserActionMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull("Activity didn't start", activity);
        Assert.assertTrue("Wrong activity started", activity instanceof BrowserActionActivity);
        instrumentation.removeMonitor(browserActionMonitor);
        ((BrowserActionActivity) activity)
                .getHelperForTesting()
                .setTestDelegateForTesting(mTestDelegate);
        return (BrowserActionActivity) activity;
    }

    private void sendBrowserActionIntent(Instrumentation instrumentation, String url) {
        Context context = instrumentation.getTargetContext();
        Intent intent = new Intent(BrowserActionsIntent.ACTION_BROWSER_ACTIONS_OPEN);
        intent.setData(Uri.parse(url));
        intent.putExtra(BrowserActionsIntent.EXTRA_TYPE, BrowserActionsIntent.URL_TYPE_NONE);
        PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, new Intent(), 0);
        intent.putExtra(BrowserActionsIntent.EXTRA_APP_ID, pendingIntent);

        // Add a custom item.
        Intent customIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        mCustomPendingItent = PendingIntent.getActivity(context, 0, customIntent, 0);
        Bundle item = new Bundle();
        item.putString(BrowserActionsIntent.KEY_TITLE, CUSTOM_ITEM_TITLE);
        item.putParcelable(BrowserActionsIntent.KEY_ACTION, mCustomPendingItent);
        ArrayList<Bundle> items = new ArrayList<>();
        items.add(item);
        intent.putParcelableArrayListExtra(BrowserActionsIntent.EXTRA_MENU_ITEMS, items);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        intent.setClass(context, BrowserActionActivity.class);
        // Android Test Rule auto adds {@link Intent.FLAG_ACTIVITY_NEW_TASK} which violates {@link
        // BrowserActionsIntent} policy. Add an extra to skip Intent.FLAG_ACTIVITY_NEW_TASK check
        // for test.
        IntentUtils.safeStartActivity(context, intent);
    }
}
