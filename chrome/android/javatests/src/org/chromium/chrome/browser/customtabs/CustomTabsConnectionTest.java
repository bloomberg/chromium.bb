// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Application;
import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.support.customtabs.ICustomTabsCallback;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

/** Tests for CustomTabsConnection. */
public class CustomTabsConnectionTest extends InstrumentationTestCase {
    private CustomTabsConnection mCustomTabsConnection;
    private static final String URL = "http://www.google.com";
    private static final String URL2 = "https://www.android.com";
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
        mCustomTabsConnection.cleanupAll();
    }

    private ICustomTabsCallback newDummyCallback() {
        return new ICustomTabsCallback.Stub() {
            @Override
            public void onNavigationEvent(int navigationEvent, Bundle extras) {}
            @Override
            public IBinder asBinder() {
                return this;
            }
        };
    }

    /**
     * Tests that we can create a new session. Registering with a null callback
     * fails, as well as multiple sessions with the same callback.
     */
    @SmallTest
    public void testNewSession() {
        assertEquals(false, mCustomTabsConnection.newSession(null));
        ICustomTabsCallback cb = newDummyCallback();
        assertEquals(true, mCustomTabsConnection.newSession(cb));
        assertEquals(false, mCustomTabsConnection.newSession(cb));
    }

    /**
     * Tests that {@link CustomTabsConnection#warmup(long)} succeeds and can
     * be issued multiple times.
     */
    @SmallTest
    public void testCanWarmup() {
        assertEquals(true, mCustomTabsConnection.warmup(0));
        assertEquals(true, mCustomTabsConnection.warmup(0));
    }

    /**
     * Calls warmup() and mayLaunchUrl(), checks for the expected result
     * (success or failure) and returns the result code.
     */
    private ICustomTabsCallback assertWarmupAndMayLaunchUrl(
            ICustomTabsCallback cb, String url, boolean shouldSucceed) {
        mCustomTabsConnection.warmup(0);
        if (cb == null) {
            cb = newDummyCallback();
            mCustomTabsConnection.newSession(cb);
        }
        boolean succeeded = mCustomTabsConnection.mayLaunchUrl(cb, Uri.parse(url), null, null);
        assertEquals(shouldSucceed, succeeded);
        return shouldSucceed ? cb : null;
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(long, String, Bundle, List<Bundle>)}
     * returns an error when called with an invalid session ID.
     */
    @SmallTest
    public void testNoMayLaunchUrlWithInvalidSessionId() {
        assertWarmupAndMayLaunchUrl(newDummyCallback(), URL, false);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(long, String, Bundle, List<Bundle>)}
     * rejects invalid URL schemes.
     */
    @SmallTest
    public void testNoMayLaunchUrlWithInvalidScheme() {
        assertWarmupAndMayLaunchUrl(null, INVALID_SCHEME_URL, false);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(long, String, Bundle, List<Bundle>)}
     * succeeds.
     */
    @SmallTest
    public void testMayLaunchUrl() {
        assertWarmupAndMayLaunchUrl(null, URL, true);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(long, String, Bundle, List<Bundle>)}
     * can be called several times with the same, and different URLs.
     */
    @SmallTest
    public void testMultipleMayLaunchUrl() {
        ICustomTabsCallback cb = assertWarmupAndMayLaunchUrl(null, URL, true);
        assertWarmupAndMayLaunchUrl(cb, URL, true);
        assertWarmupAndMayLaunchUrl(cb, URL2, true);
    }

    /**
     * Tests that sessions are forgotten properly.
     */
    @SmallTest
    public void testForgetsSession() {
        ICustomTabsCallback cb = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.cleanupAll();
        assertWarmupAndMayLaunchUrl(cb, URL, false);
    }
}
