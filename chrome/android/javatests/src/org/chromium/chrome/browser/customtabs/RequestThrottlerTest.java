// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;

/** Tests for RequestThrottler.
 *
 * Note: tests are @UiThreadTest because RequestThrottler is not thread-safe.
 */
public class RequestThrottlerTest extends InstrumentationTestCase {
    private static final int UID = 1234;
    private static final int UID2 = 12345;
    private static final String URL = "https://www.google.com";
    private static final String URL2 = "https://www.chromium.org";

    private Context mContext;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getTargetContext();
        RequestThrottler.purgeAllEntriesForTesting(mContext);
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        RequestThrottler.purgeAllEntriesForTesting(mContext);
    }

    /** Tests that a client starts not banned. */
    @SmallTest
    @UiThreadTest
    public void testIsInitiallyNotBanned() {
        assertTrue(RequestThrottler.getForUid(mContext, UID).isPrerenderingAllowed());
    }

    /** Tests that a misbehaving client gets banned. */
    @SmallTest
    @UiThreadTest
    public void testBansUid() {
        RequestThrottler throttler = RequestThrottler.getForUid(mContext, UID);
        assertTrue(throttler.isPrerenderingAllowed());
        for (int i = 0; i < 100; i++) throttler.registerPrerenderRequest(URL);
        assertFalse(throttler.isPrerenderingAllowed());
    }

    /** Tests that the URL needs to match to avoid getting banned. */
    @SmallTest
    @UiThreadTest
    public void testBanningMatchesUrls() {
        RequestThrottler throttler = RequestThrottler.getForUid(mContext, UID);
        assertTrue(throttler.isPrerenderingAllowed());
        for (int i = 0; i < 100; i++) {
            throttler.registerPrerenderRequest(URL);
            throttler.registerPrerenderRequest(URL);
            throttler.registerSuccess(URL2);
        }
        assertFalse(throttler.isPrerenderingAllowed());
    }

    /** Tests that a client can send a lot of requests, as long as they are matched by successes. */
    @SmallTest
    @UiThreadTest
    public void testDontBanAccurateClients() {
        RequestThrottler throttler = RequestThrottler.getForUid(mContext, UID);
        assertTrue(throttler.isPrerenderingAllowed());
        for (int i = 0; i < 100; i++) {
            throttler.registerPrerenderRequest(URL);
            throttler.registerSuccess(URL);
        }
        assertTrue(throttler.isPrerenderingAllowed());
    }

    /** Tests that partially accurate clients are not banned. */
    @SmallTest
    @UiThreadTest
    public void testDontBanPartiallyAccurateClients() {
        RequestThrottler throttler = RequestThrottler.getForUid(mContext, UID);
        assertTrue(throttler.isPrerenderingAllowed());
        for (int j = 0; j < 10; j++) {
            throttler.registerPrerenderRequest(URL);
            throttler.registerPrerenderRequest(URL);
            throttler.registerSuccess(URL2);
            throttler.registerSuccess(URL);
            assertTrue(throttler.isPrerenderingAllowed());
        }
    }

    /** Tests that banning a UID doesn't ban another one. */
    @SmallTest
    @UiThreadTest
    public void testThrottlingBanIsByUid() {
        RequestThrottler throttler = RequestThrottler.getForUid(mContext, UID);
        assertTrue(throttler.isPrerenderingAllowed());
        for (int i = 0; i < 100; i++) throttler.registerPrerenderRequest(URL);
        assertFalse(throttler.isPrerenderingAllowed());
        assertTrue(RequestThrottler.getForUid(mContext, UID2).isPrerenderingAllowed());
    }
}
