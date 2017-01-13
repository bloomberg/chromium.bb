// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.SearchGeolocationDisclosureTabHelper;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/** Tests for the SearchGeolocationDisclosureInfobar. */
public class SearchGeolocationDisclosureInfoBarTest
        extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String SEARCH_PAGE = "/chrome/test/data/empty.html";
    private static final String ENABLE_NEW_DISCLOSURE_FEATURE =
            "enable-features=ConsistentOmniboxGeolocation";
    private static final String DISABLE_NEW_DISCLOSURE_FEATURE =
            "disable-features=ConsistentOmniboxGeolocation";

    private EmbeddedTestServer mTestServer;

    public SearchGeolocationDisclosureInfoBarTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

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

    @SmallTest
    @Feature({"Browser", "Main"})
    @CommandLineFlags.Add(ENABLE_NEW_DISCLOSURE_FEATURE)
    public void testInfoBarAppears() throws InterruptedException, TimeoutException {
        SearchGeolocationDisclosureTabHelper.setIgnoreUrlChecksForTesting();
        assertEquals("Wrong starting infobar count", 0, getInfoBars().size());

        // Infobar should appear when doing the first search.
        InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
        InfoBarTestAnimationListener listener = new InfoBarTestAnimationListener();
        container.setAnimationListener(listener);
        loadUrl(mTestServer.getURL(SEARCH_PAGE));
        // Note: the number of infobars is checked immediately after the URL is loaded, unlike in
        // other infobar tests where it is checked after animations have completed. This is because
        // (a) in this case it should work, as these infobars are added as part of the URL loading
        // process, and
        // (b) if this doesn't work, it is important to catch it as otherwise the checks that
        // infobars haven't being shown are invalid.
        assertEquals("Wrong infobar count after search", 1, getInfoBars().size());
        listener.addInfoBarAnimationFinished("InfoBar not added.");

        // Infobar should not appear again on the same day.
        loadUrl(mTestServer.getURL(SEARCH_PAGE));
        // There can be a delay from infobars being removed in the native InfobarManager and them
        // being removed in the Java container, so wait until the infobar has really gone.
        InfoBarUtil.waitUntilNoInfoBarsExist(getInfoBars());
        assertEquals("Wrong infobar count after search", 0, getInfoBars().size());

        // Infobar should appear again the next day.
        SearchGeolocationDisclosureTabHelper.setDayOffsetForTesting(1);
        listener = new InfoBarTestAnimationListener();
        container.setAnimationListener(listener);
        loadUrl(mTestServer.getURL(SEARCH_PAGE));
        assertEquals("Wrong infobar count after search", 1, getInfoBars().size());
        listener.addInfoBarAnimationFinished("InfoBar not added.");

        // Infobar should not appear again on the same day.
        loadUrl(mTestServer.getURL(SEARCH_PAGE));
        InfoBarUtil.waitUntilNoInfoBarsExist(getInfoBars());
        assertEquals("Wrong infobar count after search", 0, getInfoBars().size());

        // Infobar should appear again the next day.
        SearchGeolocationDisclosureTabHelper.setDayOffsetForTesting(2);
        listener = new InfoBarTestAnimationListener();
        container.setAnimationListener(listener);
        loadUrl(mTestServer.getURL(SEARCH_PAGE));
        assertEquals("Wrong infobar count after search", 1, getInfoBars().size());
        listener.addInfoBarAnimationFinished("InfoBar not added.");

        // Infobar should not appear again on the same day.
        loadUrl(mTestServer.getURL(SEARCH_PAGE));
        InfoBarUtil.waitUntilNoInfoBarsExist(getInfoBars());
        assertEquals("Wrong infobar count after search", 0, getInfoBars().size());

        // Infobar has appeared three times now, it should not appear again.
        SearchGeolocationDisclosureTabHelper.setDayOffsetForTesting(3);
        loadUrl(mTestServer.getURL(SEARCH_PAGE));
        assertEquals("Wrong infobar count after search", 0, getInfoBars().size());
    }

    @SmallTest
    @Feature({"Browser", "Main"})
    @CommandLineFlags.Add(ENABLE_NEW_DISCLOSURE_FEATURE)
    public void testInfoBarDismiss() throws InterruptedException, TimeoutException {
        SearchGeolocationDisclosureTabHelper.setIgnoreUrlChecksForTesting();
        assertEquals("Wrong starting infobar count", 0, getInfoBars().size());

        // Infobar should appear when doing the first search.
        InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
        InfoBarTestAnimationListener listener = new InfoBarTestAnimationListener();
        container.setAnimationListener(listener);
        loadUrl(mTestServer.getURL(SEARCH_PAGE));
        assertEquals("Wrong infobar count after search", 1, getInfoBars().size());
        listener.addInfoBarAnimationFinished("InfoBar not added.");

        // Dismiss the infobar.
        assertTrue(InfoBarUtil.clickCloseButton(getInfoBars().get(0)));

        // Infobar should not appear again on the same day.
        loadUrl(mTestServer.getURL(SEARCH_PAGE));
        InfoBarUtil.waitUntilNoInfoBarsExist(getInfoBars());
        assertEquals("Wrong infobar count after search", 0, getInfoBars().size());

        // Infobar should not appear the next day either, as it has been dismissed.
        SearchGeolocationDisclosureTabHelper.setDayOffsetForTesting(1);
        loadUrl(mTestServer.getURL(SEARCH_PAGE));
        assertEquals("Wrong infobar count after search", 0, getInfoBars().size());
    }

    @SmallTest
    @Feature({"Browser", "Main"})
    @CommandLineFlags.Add(ENABLE_NEW_DISCLOSURE_FEATURE)
    public void testNoInfoBarForRandomUrl() throws InterruptedException, TimeoutException {
        assertEquals("Wrong starting infobar count", 0, getInfoBars().size());

        loadUrl(mTestServer.getURL(SEARCH_PAGE));
        assertEquals("Wrong infobar count after search", 0, getInfoBars().size());
    }

    @SmallTest
    @Feature({"Browser", "Main"})
    @CommandLineFlags.Add(ENABLE_NEW_DISCLOSURE_FEATURE)
    public void testNoInfoBarInIncognito() throws InterruptedException, TimeoutException {
        SearchGeolocationDisclosureTabHelper.setIgnoreUrlChecksForTesting();
        newIncognitoTabFromMenu();
        assertEquals("Wrong starting infobar count", 0, getInfoBars().size());

        loadUrl(mTestServer.getURL(SEARCH_PAGE));
        assertEquals("Wrong infobar count after search", 0, getInfoBars().size());
    }

    @SmallTest
    @Feature({"Browser", "Main"})
    @CommandLineFlags.Add(DISABLE_NEW_DISCLOSURE_FEATURE)
    public void testInfoBarAppearsDoesntAppearWithoutFeature()
            throws InterruptedException, TimeoutException {
        SearchGeolocationDisclosureTabHelper.setIgnoreUrlChecksForTesting();
        assertEquals("Wrong starting infobar count", 0, getInfoBars().size());

        loadUrl(mTestServer.getURL(SEARCH_PAGE));
        assertEquals("Wrong infobar count after search", 0, getInfoBars().size());
    }
}
