// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.os.IBinder;
import android.os.Process;
import android.support.customtabs.ICustomTabsCallback;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

/** Tests for ClientManager. */
public class ClientManagerTest extends InstrumentationTestCase {
    private static final String URL = "https://www.android.com";
    private ClientManager mClientManager;
    private ICustomTabsCallback mCallback = new CustomTabsTestUtils.DummyCallback();
    private IBinder mSession = mCallback.asBinder();
    private int mUid = Process.myUid();

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
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
        mClientManager.newSession(mCallback, mUid, null);
        assertEquals(ClientManager.SESSION_NO_WARMUP_NOT_CALLED,
                mClientManager.getWarmupState(mSession));
    }

    @SmallTest
    public void testValidSessionOtherWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid + 1);
        mClientManager.newSession(mCallback, mUid, null);
        assertEquals(ClientManager.SESSION_NO_WARMUP_ALREADY_CALLED,
                mClientManager.getWarmupState(mSession));
    }

    @SmallTest
    public void testValidSessionWarmup() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        mClientManager.newSession(mCallback, mUid, null);
        assertEquals(ClientManager.SESSION_WARMUP, mClientManager.getWarmupState(mSession));
    }

    @SmallTest
    public void testValidSessionWarmupSeveralCalls() {
        mClientManager.recordUidHasCalledWarmup(mUid);
        mClientManager.newSession(mCallback, mUid, null);
        assertEquals(ClientManager.SESSION_WARMUP, mClientManager.getWarmupState(mSession));

        ICustomTabsCallback callback = new CustomTabsTestUtils.DummyCallback();
        IBinder session = callback.asBinder();
        mClientManager.newSession(callback, mUid, null);
        assertEquals(ClientManager.SESSION_WARMUP, mClientManager.getWarmupState(session));
    }
}
