// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hosted;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.Menu;
import android.view.MenuItem;

import com.google.android.apps.chrome.R;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.CompositorChromeActivity;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.ArrayList;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Instrumentation tests for {@link HostedActivity}.
 */
public class HostedActivityTest extends ChromeActivityTestCaseBase<HostedActivity> {

    /**
     * An empty {@link BroadcastReceiver} that exists only to make the PendingIntent to carry an
     * explicit intent. Otherwise the framework will not send it after {@link PendingIntent#send()}.
     */
    public static class DummyBroadcastReceiver extends BroadcastReceiver {
        // The url has to be copied from the instrumentation class, because a BroadcastReceiver is
        // deployed as a different package, and it cannot get access to data from the
        // instrumentation package.
        private static final String TEST_PAGE_COPY = TestHttpServerClient.getUrl(
                "chrome/test/data/android/google.html");

        @Override
        public void onReceive(Context context, Intent intent) {
            assertEquals(TEST_PAGE_COPY, intent.getDataString());
        }
    }

    private static final String
            TEST_ACTION = "org.chromium.chrome.browser.hosted.TEST_PENDING_INTENT_SENT";
    private static final String TEST_PAGE = TestHttpServerClient.getUrl(
            "chrome/test/data/android/google.html");
    private static final String TEST_MENU_TITLE = "testMenuTitle";

    private HostedActivity mActivity;

    public HostedActivityTest() {
        super(HostedActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
    }

    @Override
    protected void startActivityCompletely(Intent intent) {
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(
                HostedActivity.class.getName(), null, false);
        Activity activity = getInstrumentation().startActivitySync(intent);
        assertNotNull("Main activity did not start", activity);
        HostedActivity hostedActivity = (HostedActivity) monitor.waitForActivityWithTimeout(
                ACTIVITY_START_TIMEOUT_MS);
        assertNotNull("HostedActivity did not start", hostedActivity);
        setActivity(hostedActivity);
        mActivity = hostedActivity;
    }

    private void startHostedActivityWithIntent(Intent intent) throws InterruptedException {
        startActivityCompletely(intent);
        assertTrue("Tab never selected/initialized.",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return getActivity().getActivityTab() != null;
                    }
                }));
        Tab tab = getActivity().getActivityTab();

        assertTrue("Deferred startup never completed",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return DeferredStartupHandler.getInstance().isDeferredStartupComplete();
                    }
                }));

        assertNotNull(tab);
        assertNotNull(tab.getView());
    }

    /**
     * Creates the simplest intent that is sufficient to let {@link ChromeLauncherActivity} launch
     * the {@link HostedActivity}.
     */
    private Intent createMinimalHostedIntent() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(TEST_PAGE));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setComponent(new ComponentName(getInstrumentation().getTargetContext(),
                ChromeLauncherActivity.class));
        intent.putExtra(HostedIntentDataProvider.EXTRA_HOSTED_SESSION_ID, true);
        return intent;
    }

    /**
     * Add a bundle specifying a custom menu entry.
     * @param intent The intent to modify.
     * @return The pending intent associated with the menu entry.
     */
    private PendingIntent addMenuEntryToIntent(Intent intent) {
        Intent menuIntent = new Intent();
        menuIntent.setClass(getInstrumentation().getContext(), DummyBroadcastReceiver.class);
        menuIntent.setAction(TEST_ACTION);
        PendingIntent pi = PendingIntent.getBroadcast(getInstrumentation().getTargetContext(), 0,
                menuIntent, 0);
        Bundle bundle = new Bundle();
        bundle.putString(HostedIntentDataProvider.KEY_HOSTED_MENU_TITLE, TEST_MENU_TITLE);
        bundle.putParcelable(HostedIntentDataProvider.KEY_HOSTED_PENDING_INTENT, pi);
        ArrayList<Bundle> menuItems = new ArrayList<Bundle>();
        menuItems.add(bundle);
        intent.putParcelableArrayListExtra(HostedIntentDataProvider.EXTRA_HOSTED_MENU_ITEMS,
                menuItems);
        return pi;
    }

    private void openAppMenuAndAssertMenuShown() throws InterruptedException {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mActivity.onMenuOrKeyboardAction(R.id.show_menu, false);
            }
        });

        assertTrue("App menu was not shown", CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivity.getAppMenuHandler().isAppMenuShowing();
            }
        }));
    }

    @SmallTest
    public void testContextMenuEntriesForImage() throws InterruptedException, TimeoutException {
        startHostedActivityWithIntent(createMinimalHostedIntent());

        final int expectedMenuSize = 4;
        Menu menu = ContextMenuUtils.openContextMenu(this, getActivity().getActivityTab(), "logo");
        assertEquals(expectedMenuSize, menu.size());
        assertNotNull(menu.findItem(R.id.contextmenu_save_image));
        assertNotNull(menu.findItem(R.id.contextmenu_open_image));
        assertNotNull(menu.findItem(R.id.contextmenu_copy_image));
        assertNotNull(menu.findItem(R.id.contextmenu_copy_image_url));
    }

    @SmallTest
    public void testContextMenuEntriesForLink() throws InterruptedException, TimeoutException {
        startHostedActivityWithIntent(createMinimalHostedIntent());

        final int expectedMenuSize = 3;
        Menu menu = ContextMenuUtils.openContextMenu(this, getActivity().getActivityTab(),
                "aboutLink");
        assertEquals(expectedMenuSize, menu.size());
        assertNotNull(menu.findItem(R.id.contextmenu_copy_link_address_text));
        assertNotNull(menu.findItem(R.id.contextmenu_copy_link_text));
        assertNotNull(menu.findItem(R.id.contextmenu_save_link_as));
    }

    @SmallTest
    public void testHostedAppMenu() throws InterruptedException {
        Intent intent = createMinimalHostedIntent();
        addMenuEntryToIntent(intent);
        startHostedActivityWithIntent(intent);

        openAppMenuAndAssertMenuShown();
        final int expectedMenuSize = 4;
        Menu menu = getActivity().getAppMenuHandler().getAppMenuForTest().getMenuForTest();
        assertNotNull("App menu is not initialized: ", menu);
        assertEquals(expectedMenuSize, menu.size());
        assertNotNull(menu.findItem(R.id.forward_menu_id));
        assertNotNull(menu.findItem(R.id.info_menu_id));
        assertNotNull(menu.findItem(R.id.reload_menu_id));
        assertNotNull(menu.findItem(R.id.find_in_page_id));
        assertNotNull(menu.findItem(R.id.open_in_chrome_id));
    }

    @SmallTest
    public void testCustomMenuEntry() throws InterruptedException {
        Intent intent = createMinimalHostedIntent();
        final PendingIntent pi = addMenuEntryToIntent(intent);
        startHostedActivityWithIntent(intent);

        final OnFinishedForTest onFinished = new OnFinishedForTest(pi);
        mActivity.getIntentDataProvider().setMenuSelectionOnFinishedForTesting(onFinished);

        openAppMenuAndAssertMenuShown();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                MenuItem item = mActivity.getAppMenuPropertiesDelegate().getMenuItemForTitle(
                        TEST_MENU_TITLE);
                assertNotNull(item);
                assertTrue(mActivity.onOptionsItemSelected(item));
            }
        });

        assertTrue("Pending Intent was not sent.", CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return onFinished.isSent();
            }
        }));
    }

    @SmallTest
    public void testOpenInChrome() throws InterruptedException {
        startHostedActivityWithIntent(createMinimalHostedIntent());

        boolean isDocumentMode = FeatureUtilities.isDocumentMode(
                getInstrumentation().getTargetContext());
        String activityName;
        if (isDocumentMode) {
            activityName = DocumentActivity.class.getName();
        } else {
            activityName = ChromeTabbedActivity.class.getName();
        }
        Instrumentation.ActivityMonitor monitor = getInstrumentation().addMonitor(activityName,
                null, false);

        openAppMenuAndAssertMenuShown();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mActivity.onMenuOrKeyboardAction(R.id.open_in_chrome_id, false);
            }
        });

        final CompositorChromeActivity chromeActivity = (CompositorChromeActivity) monitor
                .waitForActivityWithTimeout(ACTIVITY_START_TIMEOUT_MS);
        assertNotNull("A normal chrome activity did not start.", chromeActivity);
        assertTrue("The normal tab was not initiated correctly.",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        Tab tab = chromeActivity.getActivityTab();
                        return tab != null && tab.getUrl().equals(TEST_PAGE);
                    }
                }));
    }

    /**
     * A helper class to monitor sending status of a {@link PendingIntent}.
     */
    private static class OnFinishedForTest implements PendingIntent.OnFinished {

        private PendingIntent mPi;
        private AtomicBoolean mIsSent = new AtomicBoolean();

        /**
         * Create an instance of {@link OnFinishedForTest}, testing the given {@link PendingIntent}.
         */
        public OnFinishedForTest(PendingIntent pi) {
            mPi = pi;
        }

        /**
         * @return Whether {@link PendingIntent#send()} has been successfully triggered.
         */
        public boolean isSent() {
            return mIsSent.get();
        }

        @Override
        public void onSendFinished(PendingIntent pendingIntent, Intent intent, int resultCode,
                String resultData, Bundle resultExtras) {
            if (pendingIntent.equals(mPi)) mIsSent.set(true);
        }
    }
}