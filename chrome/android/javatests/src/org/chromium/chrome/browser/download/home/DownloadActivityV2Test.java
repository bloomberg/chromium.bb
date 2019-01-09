// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import android.support.test.filters.MediumTest;
import android.support.test.rule.ActivityTestRule;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.download.DisableAnimationsRule;
import org.chromium.chrome.browser.download.home.list.UiUtils;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.download.ui.StubbedProvider;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivity;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.ui.test.util.UiRestriction;

import java.util.HashMap;
import java.util.Map;

/** Tests the DownloadActivity and the DownloadManagerUi. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class DownloadActivityV2Test {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsRule disableAnimationsRule = new DisableAnimationsRule();

    @Rule
    public ActivityTestRule<DummyUiActivity> mActivityTestRule =
            new ActivityTestRule<>(DummyUiActivity.class);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Profile mProfile;
    @Mock
    private Tracker mTracker;
    @Mock
    private SnackbarManager mSnackbarManager;

    private DummyUiActivity mActivity;
    private DownloadManagerCoordinator mDownloadCoordinator;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        RecordUserAction.setDisabledForTests(true);
        RecordHistogram.setDisabledForTests(true);
        UiUtils.setDisableUrlFormattingForTests(true);
        DummyUiActivity.setTestTheme(org.chromium.chrome.R.style.FullscreenWhiteActivityTheme);
    }

    @Before
    public void setUp() throws Exception {
        // TODO(yliuyliu): Write a new StubbedOfflineContentProvider for new Download UI testing.
        StubbedProvider stubbdedProvider = new StubbedProvider();
        OfflineContentAggregatorFactory.setOfflineContentProviderForTests(
                stubbdedProvider.getOfflineContentProvider());

        OfflineItem item0 = StubbedProvider.createOfflineItem(0, "20151019 07:26");
        OfflineItem item1 = StubbedProvider.createOfflineItem(1, "20151020 07:27");
        OfflineItem item2 = StubbedProvider.createOfflineItem(2, "20151021 07:28");
        OfflineItem item3 = StubbedProvider.createOfflineItem(3, "20151021 07:29");

        stubbdedProvider.getOfflineContentProvider().items.add(item0);
        stubbdedProvider.getOfflineContentProvider().items.add(item1);
        stubbdedProvider.getOfflineContentProvider().items.add(item2);
        stubbdedProvider.getOfflineContentProvider().items.add(item3);

        TrackerFactory.setTrackerForTests(mTracker);

        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE, true);
        features.put(ChromeFeatureList.DOWNLOAD_HOME_SHOW_STORAGE_INFO, true);
        features.put(ChromeFeatureList.DOWNLOAD_HOME_V2, true);
        features.put(ChromeFeatureList.OFFLINE_PAGES_PREFETCHING, true);
        ChromeFeatureList.setTestFeatures(features);

        mActivity = mActivityTestRule.getActivity();
    }

    private void setUpUi() {
        DownloadManagerUiConfig config = new DownloadManagerUiConfig.Builder()
                                                 .setIsOffTheRecord(false)
                                                 .setIsSeparateActivity(true)
                                                 .setUseNewDownloadPath(true)
                                                 .setUseNewDownloadPathThumbnails(true)
                                                 .build();
        mDownloadCoordinator =
                new DownloadManagerCoordinatorImpl(mProfile, mActivity, config, mSnackbarManager);
        mActivity.setContentView(mDownloadCoordinator.getView());

        mDownloadCoordinator.updateForUrl(UrlConstants.DOWNLOADS_URL);
    }

    @Test
    @MediumTest
    public void testLaunchingActivity() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> { setUpUi(); });

        onView(withText("page 1")).check(matches(isDisplayed()));
        onView(withText("page 2")).check(matches(isDisplayed()));
        onView(withText("page 3")).check(matches(isDisplayed()));
        onView(withText("page 4")).check(matches(isDisplayed()));
    }
}
