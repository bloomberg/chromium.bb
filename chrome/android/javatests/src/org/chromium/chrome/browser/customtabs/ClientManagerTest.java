// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.os.Process;
import android.support.customtabs.CustomTabsSessionToken;
import android.test.suitebuilder.annotation.SmallTest;

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
        mClientManager.newSession(mSession, mUid, null);
        assertEquals(ClientManager.SESSION_NO_WARMUP_NOT_CALLED,
                mClientManager.getWarmupState(mSession));
    }

    @SmallTest
    public void testValidSessionOtherWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid + 1);
        mClientManager.newSession(mSession, mUid, null);
        assertEquals(ClientManager.SESSION_NO_WARMUP_ALREADY_CALLED,
                mClientManager.getWarmupState(mSession));
    }

    @SmallTest
    public void testValidSessionWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        mClientManager.newSession(mSession, mUid, null);
        assertEquals(ClientManager.SESSION_WARMUP, mClientManager.getWarmupState(mSession));
    }

    @SmallTest
    public void testValidSessionWarmupSeveralCalls() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        mClientManager.newSession(mSession, mUid, null);
        assertEquals(ClientManager.SESSION_WARMUP, mClientManager.getWarmupState(mSession));

        CustomTabsSessionToken token = CustomTabsSessionToken.createDummySessionTokenForTesting();
        mClientManager.newSession(token, mUid, null);
        assertEquals(ClientManager.SESSION_WARMUP, mClientManager.getWarmupState(token));
    }

    @SmallTest
    public void testPredictionOutcomeSuccess() {
        assertTrue(mClientManager.newSession(mSession, mUid, null));
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL));
        assertEquals(
                ClientManager.GOOD_PREDICTION, mClientManager.getPredictionOutcome(mSession, URL));
    }

    @SmallTest
    public void testPredictionOutcomeNoPrediction() {
        assertTrue(mClientManager.newSession(mSession, mUid, null));
        mClientManager.recordUidHasCalledWarmup(mUid);
        assertEquals(
                ClientManager.NO_PREDICTION, mClientManager.getPredictionOutcome(mSession, URL));
    }

    @SmallTest
    public void testPredictionOutcomeBadPrediction() {
        assertTrue(mClientManager.newSession(mSession, mUid, null));
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL));
        assertEquals(
                ClientManager.BAD_PREDICTION,
                mClientManager.getPredictionOutcome(mSession, URL + "#fragment"));
    }

    @SmallTest
    public void testPredictionOutcomeIgnoreFragment() {
        assertTrue(mClientManager.newSession(mSession, mUid, null));
        assertTrue(mClientManager.updateStatsAndReturnWhetherAllowed(mSession, mUid, URL));
        mClientManager.setIgnoreFragmentsForSession(mSession, true);
        assertEquals(
                ClientManager.GOOD_PREDICTION,
                mClientManager.getPredictionOutcome(mSession, URL + "#fragment"));
    }
}
