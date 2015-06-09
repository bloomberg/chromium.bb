// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Application;
import android.content.Context;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Process;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

/** Tests for CustomTabsConnection. */
public class CustomTabsConnectionTest extends InstrumentationTestCase {
    private CustomTabsConnection mCustomTabsConnection;
    private static final String URL = "http://www.google.com";
    private static final String INVALID_SCHEME_URL = "intent://www.google.com";

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        mCustomTabsConnection = CustomTabsConnection.getInstance((Application) context);
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        mCustomTabsConnection.cleanup(Process.myUid());
    }

    /**
     * Tests that we can register the callback. Registering null returns an
     * error code, and multiple registrations are not allowed.
     */
    @SmallTest
    public void testFinishSetup() {
        assertTrue("It should not be possible to set a null callback.",
                mCustomTabsConnection.finishSetup(null) != 0);
        ICustomTabsConnectionCallback cb = new ICustomTabsConnectionCallback.Stub() {
            @Override
            public void onUserNavigationStarted(long sessionId, String url, Bundle extras) {}
            @Override
            public void onUserNavigationFinished(long sessionId, String url, Bundle extras) {}
            @Override
            public IBinder asBinder() {
                return this;
            }
        };
        assertEquals(0, mCustomTabsConnection.finishSetup(cb));
        assertTrue("It should not be possible to set the callback twice.",
                mCustomTabsConnection.finishSetup(cb) != 0);
    }

    /**
     * Tests that {@link CustomTabsConnection#warmup(long)} succeeds and can
     * be issued multiple times.
     */
    @SmallTest
    public void testCanWarmup() {
        assertEquals(0, mCustomTabsConnection.warmup(0));
        // Can call it several times.
        assertEquals(0, mCustomTabsConnection.warmup(0));
    }

    /**
     * Tests that the session ID is positive, multiple sessions can be created,
     * and {@link CustomTabsConnection#newSession()} doesn't always return
     * the same session ID.
     */
    @SmallTest
    public void testNewSession() {
        long sessionId = mCustomTabsConnection.newSession();
        assertTrue("Session IDs should be strictly positive.", sessionId > 0);
        assertTrue("Session IDs should be unique.",
                mCustomTabsConnection.newSession() != sessionId);
    }

    /**
     * Calls warmup() and mayLaunchUrl(), checks for the expected result
     * (success or failure) and returns the result code.
     */
    private long assertWarmupAndMayLaunchUrl(long id, String url, boolean shouldSucceed) {
        mCustomTabsConnection.warmup(0);
        long sessionId = id == 0 ? mCustomTabsConnection.newSession() : id;
        mCustomTabsConnection.mayLaunchUrl(sessionId, url, null, null);
        long result = mCustomTabsConnection.mayLaunchUrl(sessionId, url, null, null);
        if (shouldSucceed) {
            assertEquals(sessionId, result);
        } else {
            assertTrue("The result should be negative to signal failure.", result < 0);
        }
        return result;
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(long, String, Bundle, List<Bundle>)}
     * returns an error when called with an invalid session ID.
     */
    @SmallTest
    public void testNoMayLaunchUrlWithInvalidSessionId() {
        assertWarmupAndMayLaunchUrl(42, URL, false);
        assertWarmupAndMayLaunchUrl(-1, URL, false);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(long, String, Bundle, List<Bundle>)}
     * rejects invalid URL schemes.
     */
    @SmallTest
    public void testNoMayLaunchUrlWithInvalidScheme() {
        assertWarmupAndMayLaunchUrl(0, INVALID_SCHEME_URL, false);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(long, String, Bundle, List<Bundle>)}
     * succeeds.
     */
    @SmallTest
    public void testMayLaunchUrl() {
        assertWarmupAndMayLaunchUrl(0, URL, true);
    }

    /**
     * Tests that session IDs are forgotten properly.
     */
    @SmallTest
    public void testForgetsSessionId() {
        long sessionId = assertWarmupAndMayLaunchUrl(0, URL, true);
        mCustomTabsConnection.cleanup(Process.myUid());
        assertWarmupAndMayLaunchUrl(sessionId, URL, false);
    }
}
