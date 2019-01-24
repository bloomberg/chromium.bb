// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.support.test.filters.SmallTest;

import com.google.android.libraries.feed.api.lifecycle.AppLifecycleListener;
import com.google.android.libraries.feed.feedapplifecyclelistener.FeedAppLifecycleListener;
import com.google.android.libraries.feed.host.network.NetworkClient;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.feed.FeedAppLifecycle.AppLifecycleEvent;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.Map;
import java.util.concurrent.TimeoutException;

/**
 * Unit tests for {@link FeedAppLifecycle}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@EnableFeatures({ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS})
public class FeedAppLifecycleTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Mock
    private FeedScheduler mFeedScheduler;
    @Mock
    private NetworkClient mNetworkClient;
    @Mock
    private FeedOfflineIndicator mOfflineIndicator;
    @Mock
    private AppLifecycleListener mAppLifecycleListener;
    @Mock
    private Map<String, Boolean> mMockFeatureList;
    private ChromeTabbedActivity mActivity;
    private FeedAppLifecycle mAppLifecycle;
    private FeedLifecycleBridge mLifecycleBridge;
    private final String mHistogramAppLifecycleEvents =
            "ContentSuggestions.Feed.AppLifecycle.Events";

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        when(mMockFeatureList.get(anyString())).thenReturn(true);
        ChromeFeatureList.setTestFeatures(mMockFeatureList);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
            } catch (ProcessInitException e) {
                Assert.fail("Native initialization failed");
            }
            Profile profile = Profile.getLastUsedProfile().getOriginalProfile();
            mLifecycleBridge = new FeedLifecycleBridge(profile);
            mAppLifecycle =
                    new FeedAppLifecycle(mAppLifecycleListener, mLifecycleBridge, mFeedScheduler);
            FeedProcessScopeFactory.createFeedProcessScopeForTesting(mFeedScheduler, mNetworkClient,
                    mOfflineIndicator, mAppLifecycle,
                    new FeedAppLifecycleListener(
                            new com.google.android.libraries.feed.api.common.ThreadUtils()),
                    new FeedLoggingBridge(profile));
        });

        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
    }

    @Test
    @SmallTest
    @Feature({"InterestFeedContentSuggestions"})
    public void testConstructionChecksActiveTabbedActivities() {
        verify(mAppLifecycleListener, times(1)).onEnterForeground();
    }

    @Test
    @SmallTest
    @Feature({"InterestFeedContentSuggestions"})
    public void testActivityStateChangesIncrementStateCounters()
            throws InterruptedException, TimeoutException {
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.ENTER_BACKGROUND, 0);
        verify(mAppLifecycleListener, times(1)).onEnterForeground();
        signalActivityStop(mActivity);
        signalActivityStart(mActivity);

        verify(mAppLifecycleListener, times(1)).onEnterBackground();
        verify(mAppLifecycleListener, times(2)).onEnterForeground();
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.ENTER_BACKGROUND, 1);
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.ENTER_FOREGROUND, 2);
    }

    @Test
    @SmallTest
    @Feature({"InterestFeedContentSuggestions"})
    public void testNtpOpeningTriggersInitializeOnlyOnce() throws InterruptedException {
        // We open to about:blank initially so we shouldn't have called initialize() yet.
        verify(mAppLifecycleListener, times(0)).initialize();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        verify(mAppLifecycleListener, times(1)).initialize();

        // Opening the NTP again shouldn't trigger another call to initialize().
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        verify(mAppLifecycleListener, times(1)).initialize();
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.INITIALIZE, 1);
    }

    @Test
    @SmallTest
    @Feature({"InterestFeedContentSuggestions"})
    public void testOnHistoryDeleted() {
        verify(mAppLifecycleListener, times(0)).onClearAll();
        verify(mAppLifecycleListener, times(0)).onClearAllWithRefresh();
        verify(mFeedScheduler, times(0)).onArticlesCleared(anyBoolean());
        // Note that typically calling onArticlesCleared(true) will not return true, but the
        // FeedAppLifecycle should not necessarily care.
        when(mFeedScheduler.onArticlesCleared(anyBoolean())).thenReturn(true).thenReturn(false);

        mAppLifecycle.onHistoryDeleted();
        verify(mAppLifecycleListener, times(1)).onClearAllWithRefresh();
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.CLEAR_ALL, 1);
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.HISTORY_DELETED, 1);
        verify(mFeedScheduler, times(1)).onArticlesCleared(true);

        mAppLifecycle.onHistoryDeleted();
        verify(mAppLifecycleListener, times(1)).onClearAll();
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.CLEAR_ALL, 2);
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.HISTORY_DELETED, 2);
        verify(mFeedScheduler, times(2)).onArticlesCleared(true);
    }

    @Test
    @SmallTest
    @Feature({"InterestFeedContentSuggestions"})
    public void testOnCachedDataCleared() {
        verify(mAppLifecycleListener, times(0)).onClearAll();
        verify(mAppLifecycleListener, times(0)).onClearAllWithRefresh();
        verify(mFeedScheduler, times(0)).onArticlesCleared(anyBoolean());
        when(mFeedScheduler.onArticlesCleared(anyBoolean())).thenReturn(true).thenReturn(false);

        mAppLifecycle.onCachedDataCleared();
        verify(mAppLifecycleListener, times(1)).onClearAllWithRefresh();
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.CLEAR_ALL, 1);
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.CACHED_DATA_CLEARED, 1);
        verify(mFeedScheduler, times(1)).onArticlesCleared(false);

        mAppLifecycle.onCachedDataCleared();
        verify(mAppLifecycleListener, times(1)).onClearAll();
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.CLEAR_ALL, 2);
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.CACHED_DATA_CLEARED, 2);
        verify(mFeedScheduler, times(2)).onArticlesCleared(false);
    }

    @Test
    @SmallTest
    @Feature({"InterestFeedContentSuggestions"})
    public void testOnSignedOut() {
        verify(mAppLifecycleListener, times(0)).onClearAll();
        verify(mAppLifecycleListener, times(0)).onClearAllWithRefresh();
        verify(mFeedScheduler, times(0)).onArticlesCleared(anyBoolean());
        when(mFeedScheduler.onArticlesCleared(anyBoolean())).thenReturn(true).thenReturn(false);

        mAppLifecycle.onSignedOut();
        verify(mAppLifecycleListener, times(1)).onClearAllWithRefresh();
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.CLEAR_ALL, 1);
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.SIGN_OUT, 1);
        verify(mFeedScheduler, times(1)).onArticlesCleared(false);

        mAppLifecycle.onSignedOut();
        verify(mAppLifecycleListener, times(1)).onClearAll();
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.CLEAR_ALL, 2);
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.SIGN_OUT, 2);
        verify(mFeedScheduler, times(2)).onArticlesCleared(false);
    }

    @Test
    @SmallTest
    @Feature({"InterestFeedContentSuggestions"})
    public void testOnSignedIn() {
        verify(mAppLifecycleListener, times(0)).onClearAll();
        verify(mAppLifecycleListener, times(0)).onClearAllWithRefresh();
        verify(mFeedScheduler, times(0)).onArticlesCleared(anyBoolean());
        when(mFeedScheduler.onArticlesCleared(anyBoolean())).thenReturn(true).thenReturn(false);

        mAppLifecycle.onSignedIn();
        verify(mAppLifecycleListener, times(1)).onClearAllWithRefresh();
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.CLEAR_ALL, 1);
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.SIGN_IN, 1);
        verify(mFeedScheduler, times(1)).onArticlesCleared(false);

        mAppLifecycle.onSignedIn();
        verify(mAppLifecycleListener, times(1)).onClearAll();
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.CLEAR_ALL, 2);
        verifyHistogram(mHistogramAppLifecycleEvents, AppLifecycleEvent.SIGN_IN, 2);
        verify(mFeedScheduler, times(2)).onArticlesCleared(false);
    }

    @Test
    @SmallTest
    @Feature({"InterestFeedContentSuggestions"})
    public void testSecondWindowDoesNotTriggerForegroundOrBackground()
            throws InterruptedException, TimeoutException {
        verify(mAppLifecycleListener, times(1)).onEnterForeground();

        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        ChromeTabbedActivity activity2 = MultiWindowTestHelper.createSecondChromeTabbedActivity(
                mActivity, new LoadUrlParams(UrlConstants.NTP_URL));

        signalActivityStop(activity2);

        // Starting and then stopping the second activity shouldn't trigger either foreground or
        // background.
        verify(mAppLifecycleListener, times(0)).onEnterBackground();
        verify(mAppLifecycleListener, times(1)).onEnterForeground();

        signalActivityStop(mActivity);

        // Only now that both TabbedActivity instances have stopped should we get a background
        // event.
        verify(mAppLifecycleListener, times(1)).onEnterBackground();
    }

    @Test
    @SmallTest
    @Feature({"InterestFeedContentSuggestions"})
    public void testMultiWindowDoesNotCauseMultipleInitialize() throws InterruptedException {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        verify(mAppLifecycleListener, times(1)).initialize();

        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        ChromeTabbedActivity activity2 = MultiWindowTestHelper.createSecondChromeTabbedActivity(
                mActivity, new LoadUrlParams(UrlConstants.NTP_URL));

        verify(mAppLifecycleListener, times(1)).initialize();
    }

    @Test
    @SmallTest
    @Feature({"InterestFeedContentSuggestions"})
    public void testResumeTriggersSchedulerForegrounded()
            throws InterruptedException, TimeoutException {
        verify(mFeedScheduler, times(1)).onForegrounded();
        signalActivityResume(mActivity);
        verify(mFeedScheduler, times(2)).onForegrounded();
    }

    @Test
    @SmallTest
    @Feature({"InterestFeedContentSuggestions"})
    public void testClearDataAfterDisablingDoesNotCrash() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            FeedProcessScopeFactory.clearFeedProcessScopeForTesting();
            PrefServiceBridge.getInstance().setBoolean(Pref.NTP_ARTICLES_SECTION_ENABLED, false);
            FeedLifecycleBridge.onCachedDataCleared();
            FeedLifecycleBridge.onHistoryDeleted();
        });
    }

    private void signalActivityStart(Activity activity)
            throws InterruptedException, TimeoutException {
        signalActivityState(activity, ActivityState.STARTED);
    }

    private void signalActivityResume(Activity activity)
            throws InterruptedException, TimeoutException {
        signalActivityState(activity, ActivityState.RESUMED);
    }

    private void signalActivityStop(Activity activity)
            throws InterruptedException, TimeoutException {
        signalActivityState(activity, ActivityState.STOPPED);
    }

    private void signalActivityState(final Activity activity,
            final @ActivityState int activityState) throws InterruptedException, TimeoutException {
        final CallbackHelper waitForStateChangeHelper = new CallbackHelper();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                ApplicationStatus.onStateChangeForTesting(activity, activityState);
                waitForStateChangeHelper.notifyCalled();
            }
        });

        waitForStateChangeHelper.waitForCallback(0);
    }

    private void verifyHistogram(String name, @AppLifecycleEvent int sample, int expectedCount) {
        assertEquals(expectedCount, RecordHistogram.getHistogramValueCountForTesting(name, sample));
    }
}
