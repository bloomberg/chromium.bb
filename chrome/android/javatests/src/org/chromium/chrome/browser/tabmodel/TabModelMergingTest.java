// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.support.test.filters.LargeTest;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtilsTest;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreTest.MockTabPersistentStoreObserver;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Tests merging tab models for Android N+ multi-instance.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
@MinAndroidSdkLevel(24)
public class TabModelMergingTest extends ChromeTabbedActivityTestBase {

    private static final String TEST_URL_0 = UrlUtils.encodeHtmlDataUri("<html>test_url_0.</html>");
    private static final String TEST_URL_1 = UrlUtils.encodeHtmlDataUri("<html>test_url_1.</html>");
    private static final String TEST_URL_2 = UrlUtils.encodeHtmlDataUri("<html>test_url_2.</html>");
    private static final String TEST_URL_3 = UrlUtils.encodeHtmlDataUri("<html>test_url_3.</html>");
    private static final String TEST_URL_4 = UrlUtils.encodeHtmlDataUri("<html>test_url_4.</html>");
    private static final String TEST_URL_5 = UrlUtils.encodeHtmlDataUri("<html>test_url_5.</html>");
    private static final String TEST_URL_6 = UrlUtils.encodeHtmlDataUri("<html>test_url_6.</html>");

    private ChromeTabbedActivity mActivity1;
    private ChromeTabbedActivity mActivity2;
    private int mActivity1State;
    private int mActivity2State;
    private String[] mMergeIntoActivity1ExpectedTabs;
    private String[] mMergeIntoActivity2ExpectedTabs;

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        // Make sure file migrations don't run as they are unnecessary since app data was cleared.
        ContextUtils.getAppSharedPreferences().edit().putBoolean(
                TabbedModeTabPersistencePolicy.PREF_HAS_RUN_FILE_MIGRATION, true).apply();
        ContextUtils.getAppSharedPreferences().edit().putBoolean(
                TabbedModeTabPersistencePolicy.PREF_HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION, true)
                        .apply();

        // Some of the logic for when to trigger a merge depends on whether the activity is in
        // multi-window mode. Set isInMultiWindowMode to true to avoid merging unexpectedly.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);

        // Initialize activities.
        mActivity1 = getActivity();
        mActivity2 = MultiWindowUtilsTest.createSecondChromeTabbedActivity(mActivity1);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivity2.getTabModelSelector().isTabStateInitialized();
            }
        });

        // Create a few tabs in each activity.
        createTabsOnUiThread();

        // Initialize activity states and register for state change events.
        mActivity1State = ApplicationStatus.getStateForActivity(mActivity1);
        mActivity2State = ApplicationStatus.getStateForActivity(mActivity2);
        ApplicationStatus.registerStateListenerForAllActivities(new ActivityStateListener() {
            @Override
            public void onActivityStateChange(Activity activity, int newState) {
                if (activity.equals(mActivity1)) {
                    mActivity1State = newState;
                } else if (activity.equals(mActivity2)) {
                    mActivity2State = newState;
                }
            }
        });
    }

    /**
     * Creates new tabs in both ChromeTabbedActivity and ChromeTabbedActivity2 and asserts that each
     * has the expected number of tabs.
     */
    private void createTabsOnUiThread() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Create normal tabs.
                mActivity1.getTabCreator(false).createNewTab(new LoadUrlParams(TEST_URL_0),
                        TabLaunchType.FROM_CHROME_UI, null);
                mActivity1.getTabCreator(false).createNewTab(new LoadUrlParams(TEST_URL_1),
                        TabLaunchType.FROM_CHROME_UI, null);
                mActivity1.getTabCreator(false).createNewTab(new LoadUrlParams(TEST_URL_2),
                        TabLaunchType.FROM_CHROME_UI, null);
                mActivity2.getTabCreator(false).createNewTab(new LoadUrlParams(TEST_URL_3),
                        TabLaunchType.FROM_CHROME_UI, null);
                mActivity2.getTabCreator(false).createNewTab(new LoadUrlParams(TEST_URL_4),
                        TabLaunchType.FROM_CHROME_UI, null);
            }
        });

        // ChromeTabbedActivity should have four normal tabs, the one it started with and the three
        // just created.
        assertEquals("Wrong number of tabs in ChromeTabbedActivity", 4,
                mActivity1.getTabModelSelector().getModel(false).getCount());

        // ChromeTabbedActivity2 should have three normal tabs, the one it started with and the two
        // just created.
        assertEquals("Wrong number of tabs in ChromeTabbedActivity2", 3,
                mActivity2.getTabModelSelector().getModel(false).getCount());

        // Construct expected tabs.
        mMergeIntoActivity1ExpectedTabs = new String[7];
        mMergeIntoActivity2ExpectedTabs = new String[7];
        for (int i = 0; i < 4; i++) {
            mMergeIntoActivity1ExpectedTabs[i] =
                    mActivity1.getTabModelSelector().getModel(false).getTabAt(i).getUrl();
            mMergeIntoActivity2ExpectedTabs[i + 3] =
                    mActivity1.getTabModelSelector().getModel(false).getTabAt(i).getUrl();
        }
        for (int i = 0; i < 3; i++) {
            mMergeIntoActivity2ExpectedTabs[i] =
                    mActivity2.getTabModelSelector().getModel(false).getTabAt(i).getUrl();
            mMergeIntoActivity1ExpectedTabs[i + 4] =
                    mActivity2.getTabModelSelector().getModel(false).getTabAt(i).getUrl();
        }
    }

    private void mergeTabsAndAssert(final ChromeTabbedActivity activity,
            final String[] expectedTabUrls) {
        String selectedTabUrl = activity.getTabModelSelector().getCurrentTab().getUrl();
        mergeTabsAndAssert(activity, expectedTabUrls, expectedTabUrls.length, selectedTabUrl);
    }

    /**
     * Merges tabs into the provided activity and asserts that the tab model looks as expected.
     * @param activity The activity to merge into.
     * @param expectedTabUrls The expected ordering of normal tab URLs after the merge.
     * @param expectedNumberOfTabs The expected number of tabs after the merge.
     * @param expectedSelectedTabUrl The expected URL of the selected tab after the merge.
     */
    private void mergeTabsAndAssert(final ChromeTabbedActivity activity,
            final String[] expectedTabUrls, final int expectedNumberOfTabs,
            String expectedSelectedTabUrl) {
        // Merge tabs into the activity.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                activity.maybeMergeTabs();
            }
        });

        // Wait for all tabs to be merged into the activity.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return activity.getTabModelSelector().getTotalTabCount() == expectedNumberOfTabs;
            }
        });

        assertTabModelMatchesExpectations(activity, expectedSelectedTabUrl, expectedTabUrls);
    }

    /**
     * @param context The context to use when creating the intent.
     * @return An intent that can be used to start a new ChromeTabbedActivity.
     */
    private Intent createChromeTabbedActivityIntent(Context context) {
        Intent intent = new Intent();
        intent.setClass(context, ChromeTabbedActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    /**
     * Starts a new ChromeTabbedActivity and asserts that the tab model looks as expected.
     * @param intent The intent to launch the new activity.
     * @param expectedSelectedTabUrl The URL of the tab that's expected to be selected.
     * @param expectedTabUrls The expected ordering of tab URLs after the merge.
     * @return The newly created ChromeTabbedActivity.
     */
    private ChromeTabbedActivity startNewChromeTabbedActivityAndAssert(Intent intent,
            String expectedSelectedTabUrl, String[] expectedTabUrls) {
        final ChromeTabbedActivity newActivity =
                (ChromeTabbedActivity) getInstrumentation().startActivitySync(intent);

        // Wait for the tab state to be initialized.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return newActivity.getTabModelSelector().isTabStateInitialized();
            }
        });

        assertTabModelMatchesExpectations(newActivity, expectedSelectedTabUrl, expectedTabUrls);

        return newActivity;
    }

    private void assertTabModelMatchesExpectations(final ChromeTabbedActivity activity,
            String expectedSelectedTabUrl, final String[] expectedTabUrls) {
        // Assert there are the correct number of tabs.
        assertEquals("Wrong number of normal tabs", expectedTabUrls.length,
                activity.getTabModelSelector().getModel(false).getCount());

        // Assert that the correct tab is selected.
        assertEquals("Wrong tab selected", expectedSelectedTabUrl,
                activity.getTabModelSelector().getCurrentTab().getUrl());

        // Assert that tabs are in the correct order.
        for (int i = 0; i < expectedTabUrls.length; i++) {
            assertEquals("Wrong tab at position " + i, expectedTabUrls[i],
                    activity.getTabModelSelector().getModel(false).getTabAt(i).getUrl());
        }
    }

    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    public void testMergeIntoChromeTabbedActivity1() throws Exception {
        mergeTabsAndAssert(mActivity1, mMergeIntoActivity1ExpectedTabs);
        mActivity1.finishAndRemoveTask();
    }

    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    public void testMergeIntoChromeTabbedActivity2() throws Exception {
        mergeTabsAndAssert(mActivity2, mMergeIntoActivity2ExpectedTabs);
        mActivity2.finishAndRemoveTask();
    }

    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    public void testMergeOnColdStart() throws Exception {
        String expectedSelectedUrl = mActivity1.getTabModelSelector().getCurrentTab().getUrl();

        // Create an intent to launch a new ChromeTabbedActivity.
        Intent intent = createChromeTabbedActivityIntent(mActivity1);

        // Save state.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivity1.saveState();
                mActivity2.saveState();
            }
        });

        // Destroy both activities.
        mActivity1.finishAndRemoveTask();
        mActivity2.finishAndRemoveTask();
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivity1State == ActivityState.DESTROYED
                        && mActivity2State == ActivityState.DESTROYED;
            }
        });

        // Start the new ChromeTabbedActivity.
        final ChromeTabbedActivity newActivity = startNewChromeTabbedActivityAndAssert(
                intent, expectedSelectedUrl, mMergeIntoActivity1ExpectedTabs);

        // Clean up.
        newActivity.finishAndRemoveTask();
    }

    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    public void testMergeOnColdStartFromChromeTabbedActivity2() throws Exception {
        String expectedSelectedUrl = mActivity2.getTabModelSelector().getCurrentTab().getUrl();

        MockTabPersistentStoreObserver mockObserver = new MockTabPersistentStoreObserver();
        TabModelSelectorImpl tabModelSelector =
                (TabModelSelectorImpl) mActivity2.getTabModelSelector();
        tabModelSelector.getTabPersistentStoreForTesting().setObserverForTesting(mockObserver);

        // Merge tabs into ChromeTabbedActivity2. Wait for the merge to finish, ensuring the
        // tab metadata file for ChromeTabbedActivity gets deleted before attempting to merge
        // on cold start.
        mergeTabsAndAssert(mActivity2, mMergeIntoActivity2ExpectedTabs);
        mockObserver.stateMergedCallback.waitForCallback(0, 1);

        // Create an intent to launch a new ChromeTabbedActivity.
        Intent intent = createChromeTabbedActivityIntent(mActivity2);

        // Destroy ChromeTabbedActivity2. ChromeTabbedActivity should have been destroyed during the
        // merge.
        mActivity2.finishAndRemoveTask();
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivity1State == ActivityState.DESTROYED
                        && mActivity2State == ActivityState.DESTROYED;
            }
        });

        // Start the new ChromeTabbedActivity.
        final ChromeTabbedActivity newActivity = startNewChromeTabbedActivityAndAssert(
                intent, expectedSelectedUrl, mMergeIntoActivity2ExpectedTabs);

        // Clean up.
        newActivity.finishAndRemoveTask();
    }

    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    @Restriction({ChromeRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testMergeWhileInTabSwitcher() throws Exception {
        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                mActivity1.getLayoutManager(), true, false);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivity1.getLayoutManager().showOverview(false);
            }
        });
        overviewModeWatcher.waitForBehavior();

        mergeTabsAndAssert(mActivity1, mMergeIntoActivity1ExpectedTabs);
        assertTrue("Overview mode should still be showing", mActivity1.isInOverviewMode());
        mActivity1.finishAndRemoveTask();
    }

    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    public void testMergeWithNoTabs() throws Exception {
        // Close all tabs and wait for the callback.
        ChromeTabUtils.closeAllTabs(getInstrumentation(), mActivity1);

        String[] expectedTabUrls = new String[3];
        for (int i = 0; i < 3; i++) {
            expectedTabUrls[i] = mMergeIntoActivity2ExpectedTabs[i];
        }

        // The first tab should be selected after the merge.
        mergeTabsAndAssert(mActivity1, expectedTabUrls, 3, expectedTabUrls[0]);
        mActivity1.finishAndRemoveTask();
    }

    @LargeTest
    @Feature({"TabPersistentStore", "MultiWindow"})
    public void testMergingIncognitoTabs() throws InterruptedException {
        // Incognito tabs must be fully loaded so that their tab states are written out.
        ChromeTabUtils.fullyLoadUrlInNewTab(getInstrumentation(), mActivity1, TEST_URL_5, true);
        ChromeTabUtils.fullyLoadUrlInNewTab(getInstrumentation(), mActivity2, TEST_URL_6, true);

        // Save state.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mActivity1.saveState();
                mActivity2.saveState();
            }
        });

        assertEquals("Wrong number of incognito tabs in ChromeTabbedActivity",
                1, mActivity1.getTabModelSelector().getModel(true).getCount());
        assertEquals("Wrong number of tabs in ChromeTabbedActivity",
                5, mActivity1.getTabModelSelector().getTotalTabCount());
        assertEquals("Wrong number of incognito tabs in ChromeTabbedActivity2",
                1, mActivity2.getTabModelSelector().getModel(true).getCount());
        assertEquals("Wrong number of tabs in ChromeTabbedActivity2",
                4, mActivity2.getTabModelSelector().getTotalTabCount());

        String selectedUrl = mActivity1.getTabModelSelector().getCurrentTab().getUrl();
        mergeTabsAndAssert(mActivity1, mMergeIntoActivity1ExpectedTabs, 9, selectedUrl);
    }
}
