// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.support.test.filters.LargeTest;
import android.test.MoreAsserts;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.prerender.ExternalPrerenderHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.PrerenderTestHelper;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.TimeoutException;

/**
 * Prerender tests.
 *
 * Tests are disabled on low-end devices. These only support one renderer for performance reasons.
 */
public class PrerenderTest extends ChromeTabbedActivityTestBase {

    private EmbeddedTestServer mTestServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * We are using Autocomplete Action Predictor to decide whether or not to prerender.
     * Without any training data the default action should be no-prerender.
     */
    @LargeTest
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"TabContents"})
    public void testNoPrerender() throws InterruptedException {
        String testUrl = mTestServer.getURL(
                "/chrome/test/data/android/prerender/google.html");
        final Tab tab = getActivity().getActivityTab();

        // Mimic user behavior: touch to focus then type some URL.
        // Since this is a URL, it should be prerendered.
        // Type one character at a time to properly simulate input
        // to the action predictor.
        typeInOmnibox(testUrl, true);

        assertFalse("URL should not have been prerendered.",
                PrerenderTestHelper.waitForPrerenderUrl(tab, testUrl, true));
        // Navigate should not use the prerendered version.
        assertEquals(TabLoadStatus.DEFAULT_PAGE_LOAD,
                loadUrlInTab(testUrl, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab));
    }

    /*
    @LargeTest
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"TabContents"})
    */
    @FlakyTest(message = "crbug.com/339668")
    public void testPrerenderNotDead() throws InterruptedException, TimeoutException {
        String testUrl = mTestServer.getURL(
                "/chrome/test/data/android/prerender/google.html");
        final Tab tab = getActivity().getActivityTab();
        PrerenderTestHelper.prerenderUrl(testUrl, tab);
        // Navigate should use the prerendered version.
        assertEquals(TabLoadStatus.FULL_PRERENDERED_PAGE_LOAD, loadUrl(testUrl));

        // Prerender again with new text; make sure we get something different.
        String newTitle = "Welcome to the YouTube";
        testUrl = mTestServer.getURL("/chrome/test/data/android/prerender/youtube.html");
        PrerenderTestHelper.prerenderUrl(testUrl, tab);

        // Make sure the current tab title is NOT from the prerendered page.
        MoreAsserts.assertNotEqual(newTitle, tab.getTitle());

        TabTitleObserver observer = new TabTitleObserver(tab, newTitle);

        // Now commit and see the new title.
        loadUrl(testUrl);

        observer.waitForTitleUpdate(5);
        assertEquals(newTitle, tab.getTitle());
    }

    /**
     * Tests that we do get the page load finished notification even when a page has been fully
     * prerendered.
     */
    @LargeTest
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"TabContents"})
    @RetryOnFailure
    public void testPageLoadFinishNotification() throws InterruptedException {
        String url = mTestServer.getURL("/chrome/test/data/android/prerender/google.html");
        PrerenderTestHelper.prerenderUrl(url, getActivity().getActivityTab());
        loadUrl(url);
    }

    /**
     * Tests that we don't crash when dismissing a prerendered page with infobars and unload
     * handler (See bug 5757331).
     * Note that this bug happened with the instant code. Now that we use Wicked Fast, we don't
     * deal with infobars ourselves.
     */
    /*
    @LargeTest
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"TabContents"})
    */
    @DisabledTest(message = "Prerenderer disables infobars. crbug.com/588808")
    public void testInfoBarDismissed() throws InterruptedException {
        final String url = mTestServer.getURL(
                "/chrome/test/data/geolocation/geolocation_on_load.html");
        final ExternalPrerenderHandler handler =
                PrerenderTestHelper.prerenderUrl(url, getActivity().getActivityTab());

        // Cancel the prerender. This will discard the prerendered WebContents and close the
        // infobars.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                handler.cancelCurrentPrerender();
            }
        });
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
