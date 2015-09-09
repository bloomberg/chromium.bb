// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prerender;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.Callable;

/**
 * Tests for adding and removing prerenders using the {@link ExternalPrerenderHandler}
 */
public class ExternalPrerenderRequestTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String GOOGLE_URL =
            TestHttpServerClient.getUrl("chrome/test/data/android/prerender/google.html");
    private static final String YOUTUBE_URL =
            TestHttpServerClient.getUrl("chrome/test/data/android/prerender/youtube.html");
    private static final int PRERENDER_DELAY_MS = 500;

    private ExternalPrerenderHandler mHandler;
    private Profile mProfile;

    public ExternalPrerenderRequestTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mHandler = new ExternalPrerenderHandler();
        final Callable<Profile> profileCallable = new Callable<Profile>() {
            @Override
            public Profile call() throws Exception {
                return Profile.getLastUsedProfile();
            }
        };
        mProfile = ThreadUtils.runOnUiThreadBlocking(profileCallable);
    }

    @MediumTest
    @UiThreadTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Prerender"})
    /**
     * Test adding a prerender and canceling that to add a new one.
     */
    public void testAddPrerenderAndCancel() throws InterruptedException {
        WebContents webContents = mHandler.addPrerender(mProfile, GOOGLE_URL, "", 0, 0);
        assertTrue(ExternalPrerenderHandler.hasPrerenderedUrl(
                mProfile, GOOGLE_URL, webContents));

        mHandler.cancelCurrentPrerender();
        assertFalse(ExternalPrerenderHandler.hasPrerenderedUrl(
                mProfile, GOOGLE_URL, webContents));
        webContents.destroy();
        Thread.sleep(PRERENDER_DELAY_MS);
        webContents = mHandler.addPrerender(mProfile, YOUTUBE_URL, "", 0, 0);
        assertTrue(ExternalPrerenderHandler.hasPrerenderedUrl(
                mProfile, YOUTUBE_URL, webContents));
    }

    @SmallTest
    @UiThreadTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Prerender"})
    /**
     * Test calling cancel without any added prerenders.
     */
    public void testCancelPrerender() {
        mHandler.cancelCurrentPrerender();
        WebContents webContents = mHandler.addPrerender(mProfile, GOOGLE_URL, "", 0, 0);
        assertTrue(ExternalPrerenderHandler.hasPrerenderedUrl(
                mProfile, GOOGLE_URL, webContents));
    }

    @MediumTest
    @UiThreadTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Prerender"})
    /**
     * Test adding two prerenders without canceling the first one.
     */
    public void testAddingPrerendersInaRow() throws InterruptedException {
        WebContents webContents = mHandler.addPrerender(mProfile, GOOGLE_URL, "", 0, 0);
        assertTrue(ExternalPrerenderHandler.hasPrerenderedUrl(
                mProfile, GOOGLE_URL, webContents));
        Thread.sleep(PRERENDER_DELAY_MS);
        WebContents newWebContents = mHandler.addPrerender(mProfile, YOUTUBE_URL, "", 0, 0);
        assertTrue(ExternalPrerenderHandler.hasPrerenderedUrl(
                mProfile, YOUTUBE_URL, newWebContents));
    }
}
