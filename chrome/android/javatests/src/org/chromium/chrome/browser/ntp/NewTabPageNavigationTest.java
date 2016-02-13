// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.os.Environment;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.Smoke;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests loading the NTP and navigating between it and other pages.
 */
public class NewTabPageNavigationTest extends ChromeTabbedActivityTestBase {

    private EmbeddedTestServer mTestServer;

    public NewTabPageNavigationTest() {
        mSkipCheckHttpServer = true;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Sanity check that we do start on the NTP by default.
     */
    @Smoke
    @MediumTest
    @Feature({"NewTabPage", "Main"})
    public void testNTPIsDefault() {
        Tab tab = getActivity().getActivityTab();
        assertNotNull(tab);
        String url = tab.getUrl();
        assertTrue("Unexpected url: " + url,
                url.startsWith("chrome-native://newtab/")
                || url.startsWith("chrome-native://bookmarks/")
                || url.startsWith("chrome-native://recent-tabs/"));
    }

    /**
     * Check that navigating away from the NTP does work.
     */
    @LargeTest
    @Feature({"NewTabPage"})
    public void testNavigatingFromNTP() throws InterruptedException {
        String url = mTestServer.getURL("/chrome/test/data/android/google.html");
        loadUrl(url);
        assertEquals(url, getActivity().getActivityTab().getUrl());
    }

    /**
     * Tests navigating back to the NTP after loading another page.
     */
    @MediumTest
    @Feature({"NewTabPage"})
    public void testNavigateBackToNTPViaUrl() throws InterruptedException {
        String url = mTestServer.getURL("/chrome/test/data/android/google.html");
        loadUrl(url);
        assertEquals(url, getActivity().getActivityTab().getUrl());

        loadUrl(UrlConstants.NTP_URL);
        Tab tab = getActivity().getActivityTab();
        assertNotNull(tab);
        url = tab.getUrl();
        assertEquals(UrlConstants.NTP_URL, url);

        // Check that the NTP is actually displayed.
        assertNotNull(tab.getNativePage() instanceof NewTabPage);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        // Passing null below starts the activity on its default page, which is the NTP on a clean
        // profile.
        startMainActivityWithURL(null);
    }
}
