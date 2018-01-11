// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests showing the download manager when Chrome Home is enabled.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
@CommandLineFlags.Add({
        "enable-features=ChromeHome", ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
public class ChromeHomeDownloadManagerTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";

    @Rule
    public BottomSheetTestRule mBottomSheetTestRule = new BottomSheetTestRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private EmbeddedTestServer mTestServer;
    private String mTestPage;

    @Before
    public void setUp() throws Exception {
        Context appContext = InstrumentationRegistry.getInstrumentation()
                                     .getTargetContext()
                                     .getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    public void testShowDownloadManagerNoActivitiesRunning() {
        Assert.assertNull(ApplicationStatus.getLastTrackedFocusedActivity());

        showDownloadManager();

        CriteriaHelper.pollUiThread(
                new Criteria("DownloadActivity should be last tracked focused activity.") {
                    @Override
                    public boolean isSatisfied() {
                        return ApplicationStatus.getLastTrackedFocusedActivity()
                                       instanceof DownloadActivity;
                    }
                });
    }

    @Test
    @SmallTest
    public void testShowDownloadManagerLastTrackedActivityCustomTab_TabbedActivityNotRunning() {
        startCustomTabActivity();
        showDownloadManager();

        CriteriaHelper.pollUiThread(
                new Criteria("DownloadActivity should be last tracked focused activity.") {
                    @Override
                    public boolean isSatisfied() {
                        return ApplicationStatus.getLastTrackedFocusedActivity()
                                       instanceof DownloadActivity;
                    }
                });
    }

    @Test
    @SmallTest
    public void testShowDownloadManagerLastTrackedActivityCustomTab_TabbedActivityRunning()
            throws InterruptedException {
        startTabbedActivity();
        startCustomTabActivity();
        showDownloadManager();
        assertBottomSheetShowingDownloadsManager();
    }

    @Test
    @SmallTest
    public void testShowDownloadManagerLastTrackedActivityNull_TabbedActivityRunning()
            throws InterruptedException {
        startTabbedActivity();
        startCustomTabActivity();

        // Go to the home screen so that the ChromeTabbedActivity does not resume when the
        // CustomTabActivity is killed.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Intent intent = new Intent(Intent.ACTION_MAIN);
            intent.addCategory(Intent.CATEGORY_HOME);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            InstrumentationRegistry.getTargetContext().startActivity(intent);
        });

        // Finish the custom tab activity and assert that the last tracked focused activity is null.
        ThreadUtils.runOnUiThreadBlocking(
                () -> { ApplicationStatus.getLastTrackedFocusedActivity().finish(); });
        CriteriaHelper.pollUiThread(new Criteria("Last tracked focused activity should be null.") {
            @Override
            public boolean isSatisfied() {
                return ApplicationStatus.getLastTrackedFocusedActivity() == null;
            }
        });

        showDownloadManager();
        assertBottomSheetShowingDownloadsManager();
    }

    private void startTabbedActivity() throws InterruptedException {
        mBottomSheetTestRule.startMainActivityOnBlankPage();
        mBottomSheetTestRule.setSheetState(BottomSheet.SHEET_STATE_PEEK, false);
    }

    private void startCustomTabActivity() {
        final Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), mTestPage);

        try {
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        } catch (InterruptedException e) {
            Assert.fail("Failed to create CustomTabActivity");
        }

        CriteriaHelper.pollUiThread(
                new Criteria("CustomTabActivity should be last focused activity.") {
                    @Override
                    public boolean isSatisfied() {
                        return ApplicationStatus.getLastTrackedFocusedActivity()
                                       instanceof CustomTabActivity;
                    }
                });
    }

    private void showDownloadManager() {
        ThreadUtils.runOnUiThreadBlocking(() -> { DownloadUtils.showDownloadManager(null, null); });
    }

    private void assertBottomSheetShowingDownloadsManager() {
        CriteriaHelper.pollUiThread(new Criteria("ChromeTabbedActivity should be resumed.") {
            @Override
            public boolean isSatisfied() {
                return ApplicationStatus.getStateForActivity(mBottomSheetTestRule.getActivity())
                        == ActivityState.RESUMED;
            }
        });

        ThreadUtils.runOnUiThreadBlocking(
                () -> { mBottomSheetTestRule.getBottomSheet().endAnimations(); });

        CriteriaHelper.pollUiThread(new Criteria("Animations sould be finished.") {
            @Override
            public boolean isSatisfied() {
                return !mBottomSheetTestRule.getBottomSheet().isRunningSettleAnimation()
                        && !mBottomSheetTestRule.getBottomSheet().isRunningContentSwapAnimation();
            }
        });

        Assert.assertEquals("Bottom sheet should be at full height", BottomSheet.SHEET_STATE_FULL,
                mBottomSheetTestRule.getBottomSheet().getSheetState());
    }
}
