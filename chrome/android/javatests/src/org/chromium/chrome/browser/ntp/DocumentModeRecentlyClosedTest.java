// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.os.Build;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.browser.document.DocumentModeTestBase;
import org.chromium.chrome.browser.document.IncognitoDocumentActivity;
import org.chromium.chrome.browser.ntp.RecentlyClosedBridge.RecentlyClosedTab;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegate;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegateImpl;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel.Entry;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.ui.WindowOpenDisposition;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests that the "Recently closed" list of Tabs in the "Recent Tabs" menu is updated properly.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
@DisableInTabbedMode
public class DocumentModeRecentlyClosedTest extends DocumentModeTestBase {

    /** Used to report that a fake Task is sitting in Android's Overview menu. */
    private static class TestActivityDelegate extends ActivityDelegateImpl {
        private Entry mExtraRegularTask;

        TestActivityDelegate() {
            super(DocumentActivity.class, IncognitoDocumentActivity.class);
        }

        @Override
        public List<Entry> getTasksFromRecents(boolean isIncognito) {
            List<Entry> entries = super.getTasksFromRecents(isIncognito);
            if (!isIncognito && mExtraRegularTask != null) {
                entries.add(mExtraRegularTask);
            }
            return entries;
        }

        @Override
        public void finishAndRemoveTask(boolean isIncognito, int tabId) {
            if (mExtraRegularTask != null && mExtraRegularTask.tabId == tabId) {
                mExtraRegularTask = null;
            } else {
                super.finishAndRemoveTask(isIncognito, tabId);
            }
        }

        @Override
        public boolean isTabAssociatedWithNonDestroyedActivity(boolean isIncognito, int tabId) {
            if (mExtraRegularTask != null && mExtraRegularTask.tabId == tabId) {
                return false;
            } else {
                return super.isTabAssociatedWithNonDestroyedActivity(isIncognito, tabId);
            }
        }

    }

    /**
     * Test that Tabs that disappear from the Overview menu (e.g. by a user swipe) are marked as
     * recently closed.  This test relies on mocking out the ActivityDelegate because we have no
     * obvious way to simulate a user closing a Tab while Chrome is closed.
     */
    @MediumTest
    public void testMissingTasksBecomeRecentlyClosed() throws Exception {
        // Set up the DocumentTabModel so that it finds a task in Android's Recents that it doesn't
        // know about, which results in adding the Tab to the DocumentTabModel.
        TestActivityDelegate activityDelegate = new TestActivityDelegate();
        activityDelegate.mExtraRegularTask = new Entry(TestTabModelDirectory.V2_TEXTAREA.tabId);
        DocumentTabModelSelector.setActivityDelegateForTests(activityDelegate);
        mStorageDelegate.addEncodedTabState(TestTabModelDirectory.V2_TEXTAREA.tabId, false,
                TestTabModelDirectory.V2_TEXTAREA.encodedTabState);

        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        assertEquals("Wrong tab count", 1, selector.getTotalTabCount());

        // Start a DocumentActivity.  Should have two tabs.
        int newActivityId = launchViaViewIntent(false, URL_1, "Page 1");
        assertEquals("Wrong tab count", 2, selector.getTotalTabCount());

        // Return to the Android Home screen.
        ApplicationTestUtils.fireHomeScreenIntent(mContext);
        assertEquals("Wrong tab count", 2, selector.getTotalTabCount());

        // Remove the fake task, then bring Chrome back to the foreground to make the
        // DocumentTabModel update.
        activityDelegate.mExtraRegularTask = null;
        ApplicationTestUtils.launchChrome(mContext);
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return lastActivity instanceof DocumentActivity;
            }
        }));

        // Wait until the DocumentTabModel updates and shows a single tab.
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return selector.getTotalTabCount() == 1;
            }
        }));

        // Check recently closed, make sure it shows one open and one closed tab.
        DocumentActivity activity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        final DocumentRecentTabsManager recentTabsManager = getRecentTabsManager(activity);
        assertEquals("Wrong open tab count", 1, recentTabsManager.getCurrentlyOpenTabs().size());
        assertEquals("Wrong tab stayed open",
                    newActivityId, recentTabsManager.getCurrentlyOpenTabs().get(0).getTabId());
        assertEquals("Wrong closed tab count",
                    1, recentTabsManager.getRecentlyClosedTabs().size());
    }

    /** Test that the "Recently closed" list is updated properly via the TabModel. */
    @MediumTest
    public void testUpdateRecentlyClosedAfterTabModelClose() throws Exception {
        int[] tabIds = launchThreeTabs();

        // Open Recent Tabs directly instead of interacting with the dialog and check that there's
        // nothing listed under Recently closed.
        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        final DocumentActivity thirdActivity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        final DocumentRecentTabsManager recentTabsManager = getRecentTabsManager(thirdActivity);
        List<CurrentlyOpenTab> beforeOpenTabs = recentTabsManager.getCurrentlyOpenTabs();
        assertEquals("Open tabs are missing", 3, beforeOpenTabs.size());
        for (CurrentlyOpenTab openTab : beforeOpenTabs) {
            boolean isListed = false;
            for (int i = 0; i < tabIds.length; i++) {
                isListed |= openTab.getTabId() == tabIds[i];
            }
            assertTrue("Tab not marked as open: " + openTab.getTabId(), isListed);
        }
        assertEquals("Wrong closed tab count", 0,
                recentTabsManager.getRecentlyClosedTabs().size());

        // Close the first tab directly via the selector.
        int closedTabId = selector.getModel(false).getTabAt(0).getId();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    selector.getModel(false).closeTabAt(0);
                }
        });
        assertTrue(CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return selector.getTotalTabCount() == 2;
                }
        }));

        // Check that the right things appear in Recent Tabs.
        List<CurrentlyOpenTab> afterOpenTabs = recentTabsManager.getCurrentlyOpenTabs();
        assertEquals("Open tabs are missing", 2, afterOpenTabs.size());
        for (CurrentlyOpenTab openTab : afterOpenTabs) {
            boolean isListed = false;
            for (int i = 0; i < tabIds.length; i++) {
                isListed |= openTab.getTabId() == tabIds[i];
            }
            assertTrue("Tab not marked as open: " + openTab.getTabId(), isListed);
            assertNotSame("Tab remained open", closedTabId, openTab.getTabId());
        }

        final List<RecentlyClosedTab> afterRecentlyClosed =
                recentTabsManager.getRecentlyClosedTabs();
        assertEquals("Wrong closed tab count", 1, afterRecentlyClosed.size());
        assertEquals("Wrong tab closed", "Page 1", afterRecentlyClosed.get(0).title);

        // Check that the recently closed Tab is reopened.
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                recentTabsManager.openRecentlyClosedTab(
                        afterRecentlyClosed.get(0), WindowOpenDisposition.NEW_FOREGROUND_TAB);
            }
        };
        DocumentActivity relaunchedActivity = ActivityUtils.waitForActivity(
                getInstrumentation(), DocumentActivity.class, runnable);
        waitForFullLoad(relaunchedActivity, "Page 1");
        assertEquals("Wrong tab count", 3, selector.getTotalTabCount());
    }

    /**
     * Test that the "Recently closed" list is updated when Chrome is alive but backgrounded.
     */
    @MediumTest
    public void testUpdateRecentlyClosedWhenChromeInBackground() throws Exception {
        int[] tabIds = launchThreeTabs();

        // Open Recent Tabs directly instead of interacting with the dialog and check that there's
        // nothing listed under Recently closed.
        final DocumentActivity thirdActivity =
                (DocumentActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        final DocumentRecentTabsManager beforeManager = getRecentTabsManager(thirdActivity);
        List<CurrentlyOpenTab> beforeOpenTabs = beforeManager.getCurrentlyOpenTabs();
        assertEquals("Open tabs are missing", 3, beforeOpenTabs.size());
        for (CurrentlyOpenTab openTab : beforeOpenTabs) {
            boolean isListed = false;
            for (int i = 0; i < tabIds.length; i++) {
                isListed |= openTab.getTabId() == tabIds[i];
            }
            assertTrue("Tab not marked as open: " + openTab.getTabId(), isListed);
        }
        assertEquals("Wrong closed tab count", 0, beforeManager.getRecentlyClosedTabs().size());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    beforeManager.destroy();
                }
        });

        // Finish the first Task.
        final DocumentTabModelSelector selector =
                ChromeApplication.getDocumentTabModelSelector();
        assertEquals("Wrong tab count", 3, selector.getTotalTabCount());
        ActivityDelegate delegate =
                new ActivityDelegateImpl(DocumentActivity.class, IncognitoDocumentActivity.class);
        delegate.finishAndRemoveTask(false, tabIds[0]);
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return selector.getTotalTabCount() == 2;
            }
        }));

        // Check that the Tab shows up under Recently closed.
        final DocumentRecentTabsManager recentTabsManager = getRecentTabsManager(thirdActivity);
        final List<RecentlyClosedTab> afterRecentlyClosed =
                recentTabsManager.getRecentlyClosedTabs();
        assertEquals("Wrong closed tab count", 1, afterRecentlyClosed.size());
        assertEquals("Wrong tab closed", "Page 1", afterRecentlyClosed.get(0).title);
    }

    /**
     * Returns a DocumentRecentTabsManager corresponding to the given DocumentActivity.
     */
    private DocumentRecentTabsManager getRecentTabsManager(final DocumentActivity activity)
            throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<DocumentRecentTabsManager>() {
            @Override
            public DocumentRecentTabsManager call() {
                return new DocumentRecentTabsManager(activity.getActivityTab(), activity);
            }
        });
    }
}
