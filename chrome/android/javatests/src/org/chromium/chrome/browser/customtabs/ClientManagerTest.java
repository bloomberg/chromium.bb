// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.os.Process;
import android.support.customtabs.CustomTabsSessionToken;
import android.support.test.filters.SmallTest;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/** Tests for ClientManager. */
public class ClientManagerTest extends NativeLibraryTestBase {
    private static final String URL = "https://www.android.com";
    private ClientManager mClientManager;
    private CustomTabsSessionToken mSession =
            CustomTabsSessionToken.createDummySessionTokenForTesting();
    private int mUid = Process.myUid();

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        loadNativeLibraryNoBrowserProcess();
        RequestThrottler.purgeAllEntriesForTesting(context);
        mClientManager = new ClientManager(context);
        RecordHistogram.initialize();
    }

    @SmallTest
    public void testNoSessionNoWarmup() {
        assertEquals(ClientManager.NO_SESSION_NO_WARMUP, mClientManager.getWarmupState(null));
    }

    @SmallTest
    public void testNoSessionWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        assertEquals(ClientManager.NO_SESSION_WARMUP, mClientManager.getWarmupState(null));
    }

    @SmallTest
    public void testInvalidSessionNoWarmup() {
        assertEquals(ClientManager.NO_SESSION_NO_WARMUP, mClientManager.getWarmupState(mSession));
    }

    @SmallTest
    public void testInvalidSessionWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        assertEquals(ClientManager.NO_SESSION_WARMUP, mClientManager.getWarmupState(mSession));
    }

    @SmallTest
    public void testValidSessionNoWarmup() {
        mClientManager.newSession(mSession, mUid, null, null);
        assertEquals(ClientManager.SESSION_NO_WARMUP_NOT_CALLED,
                mClientManager.getWarmupState(mSession));
    }

    @SmallTest
    public void testValidSessionOtherWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid + 1);
        mClientManager.newSession(mSession, mUid, null, null);
        assertEquals(ClientManager.SESSION_NO_WARMUP_ALREADY_CALLED,
                mClientManager.getWarmupState(mSession));
    }

    @SmallTest
    public void testValidSessionWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        mClientManager.newSession(mSession, mUid, null, null);
        assertEquals(ClientManager.SESSION_WARMUP, mClientManager.getWarmupState(mSession));
    }

    @SmallTest
    public void testValidSessionWarmupSeveralCalls() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        mClientManager.newSession(mSession, mUid, null, null);
        assertEquals(ClientManager.SESSION_WARMUP, mClientManager.getWarmupState(mSession));

        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        mClientManager.newSession(token, mUid, null, null);
        assertEquals(ClientManager.SESSION_WARMUP, mClientManager.getWarmupState(token));
    }

    @SmallTest
    @RetryOnFailure
    public void testPredictionOutcomeSuccess() {
        assertTrue(mClientManager.newSession(mSession, mUid, null, null));
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        assertEquals(
                ClientManager.GOOD_PREDICTION, mClientManager.getPredictionOutcome(mSession, URL));
    }

    @SmallTest
    public void testPredictionOutcomeNoPrediction() {
        assertTrue(mClientManager.newSession(mSession, mUid, null, null));
        mClientManager.recordUidHasCalledWarmup(mUid);
        assertEquals(
                ClientManager.NO_PREDICTION, mClientManager.getPredictionOutcome(mSession, URL));
    }

    @SmallTest
    public void testPredictionOutcomeBadPrediction() {
        assertTrue(mClientManager.newSession(mSession, mUid, null, null));
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        assertEquals(
                ClientManager.BAD_PREDICTION,
                mClientManager.getPredictionOutcome(mSession, URL + "#fragment"));
    }

    @SmallTest
    public void testPredictionOutcomeIgnoreFragment() {
        assertTrue(mClientManager.newSession(mSession, mUid, null, null));
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        mClientManager.setIgnoreFragmentsForSession(mSession, true);
        assertEquals(
                ClientManager.GOOD_PREDICTION,
                mClientManager.getPredictionOutcome(mSession, URL + "#fragment"));
    }

    @SmallTest
    public void testFirstLowConfidencePredictionIsNotThrottled() {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        assertTrue(mClientManager.newSession(mSession, mUid, null, null));

        // Two low confidence in a row is OK.
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, null, true));
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, null, true));
        mClientManager.registerLaunch(mSession, URL);

        // Low -> High as well.
        RequestThrottler.purgeAllEntriesForTesting(context);
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, null, true));
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        mClientManager.registerLaunch(mSession, URL);

        // High -> Low as well.
        RequestThrottler.purgeAllEntriesForTesting(context);
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, null, true));
        mClientManager.registerLaunch(mSession, URL);
    }

    @SmallTest
    public void testMayLaunchUrlAccounting() {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();

        String name = "CustomTabs.MayLaunchUrlType";
        MetricsUtils.HistogramDelta noMayLaunchUrlDelta =
                new MetricsUtils.HistogramDelta(name, ClientManager.NO_MAY_LAUNCH_URL);
        MetricsUtils.HistogramDelta lowConfidenceDelta =
                new MetricsUtils.HistogramDelta(name, ClientManager.LOW_CONFIDENCE);
        MetricsUtils.HistogramDelta highConfidenceDelta =
                new MetricsUtils.HistogramDelta(name, ClientManager.HIGH_CONFIDENCE);
        MetricsUtils.HistogramDelta bothDelta =
                new MetricsUtils.HistogramDelta(name, ClientManager.BOTH);

        assertTrue(mClientManager.newSession(mSession, mUid, null, null));

        // No prediction;
        mClientManager.registerLaunch(mSession, URL);
        assertEquals(1, noMayLaunchUrlDelta.getDelta());

        // Low confidence.
        RequestThrottler.purgeAllEntriesForTesting(context);
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, null, true));
        mClientManager.registerLaunch(mSession, URL);
        assertEquals(1, lowConfidenceDelta.getDelta());

        // High confidence.
        RequestThrottler.purgeAllEntriesForTesting(context);
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        mClientManager.registerLaunch(mSession, URL);
        assertEquals(1, highConfidenceDelta.getDelta());

        // Low and High confidence.
        RequestThrottler.purgeAllEntriesForTesting(context);
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, false));
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, null, true));
        mClientManager.registerLaunch(mSession, URL);
        assertEquals(1, bothDelta.getDelta());

        // Low and High confidence, same call.
        RequestThrottler.purgeAllEntriesForTesting(context);
        bothDelta = new MetricsUtils.HistogramDelta(name, ClientManager.BOTH);
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL, true));
        mClientManager.registerLaunch(mSession, URL);
        assertEquals(1, bothDelta.getDelta());
    }
}
