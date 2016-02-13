// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.os.Environment;
import android.test.FlakyTest;
import android.test.MoreAsserts;
import android.test.suitebuilder.annotation.LargeTest;
import android.view.KeyEvent;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.prerender.ExternalPrerenderHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.PrerenderTestHelper;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content.browser.test.util.KeyUtils;
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

    public PrerenderTest() {
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
     * We are using Autocomplete Action Predictor to decide whether or not to prerender.
    /* Without any training data the default action should be no-prerender.
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
    crbug.com/339668
    @LargeTest
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"TabContents"})
    */
    @FlakyTest
    public void testPrerenderNotDead() throws InterruptedException, TimeoutException {
        String testUrl = mTestServer.getURL(
                "/chrome/test/data/android/prerender/google.html");
        PrerenderTestHelper.prerenderUrlAndFocusOmnibox(testUrl, this);
        final Tab tab = getActivity().getActivityTab();
        // Navigate should use the prerendered version.
        assertEquals(TabLoadStatus.FULL_PRERENDERED_PAGE_LOAD,
                loadUrlInTab(testUrl, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab));

        // Prerender again with new text; make sure we get something different.
        String newTitle = "Welcome to the YouTube";
        testUrl = mTestServer.getURL("/chrome/test/data/android/prerender/youtube.html");
        PrerenderTestHelper.prerenderUrlAndFocusOmnibox(testUrl, this);

        // Make sure the current tab title is NOT from the prerendered page.
        MoreAsserts.assertNotEqual(newTitle, tab.getTitle());

        TabTitleObserver observer = new TabTitleObserver(tab, newTitle);

        // Now commit and see the new title.
        final UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
        assertNotNull("urlBar is null", urlBar);
        KeyUtils.singleKeyEventView(getInstrumentation(), urlBar, KeyEvent.KEYCODE_ENTER);

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
    public void testPageLoadFinishNotification() throws InterruptedException {
        String url = mTestServer.getURL("/chrome/test/data/android/prerender/google.html");
        PrerenderTestHelper.prerenderUrlAndFocusOmnibox(url, this);
        // Now let's press enter to validate the suggestion. The prerendered page should be
        // committed and we should get a page load finished notification (which would trigger the
        // page load).
        ChromeTabUtils.waitForTabPageLoaded(getActivity().getActivityTab(), new Runnable() {
            @Override
            public void run() {
                final UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
                assertNotNull("urlBar is null", urlBar);
                KeyUtils.singleKeyEventView(getInstrumentation(), urlBar, KeyEvent.KEYCODE_ENTER);
            }
        });
    }

    /**
     * Tests that we don't crash when dismissing a prerendered page with infobars and unload
     * handler (See bug 5757331).
     * Note that this bug happened with the instant code. Now that we use Wicked Fast, we don't
     * deal with infobars ourselves.
     */
    @LargeTest
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"TabContents"})
    public void testInfoBarDismissed() throws InterruptedException {
        final String url = mTestServer.getURL(
                "/chrome/test/data/geolocation/geolocation_on_load.html");
        final ExternalPrerenderHandler handler = PrerenderTestHelper.prerenderUrlAndFocusOmnibox(
                url, this);

        // Let's clear the URL bar, this will discard the prerendered WebContents and close the
        // infobars.
        final UrlBar urlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
        assertNotNull(urlBar);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                urlBar.requestFocus();
                urlBar.setText("");
                handler.cancelCurrentPrerender();
            }
        });
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
