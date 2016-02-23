// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Environment;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.Smoke;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content.browser.LocationProviderFactory;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.MockLocationProvider;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Test suite for Geo-Location functionality.
 *
 * These tests rely on the device being specially setup (which the bots do automatically):
 * - Global location is enabled.
 * - Google location is enabled.
 */
public class GeolocationTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String LOCATION_PROVIDER_MOCK = "locationProviderMock";
    private static final double LATITUDE = 51.01;
    private static final double LONGITUDE = 0.23;
    private static final float ACCURACY = 10;
    private static final String TEST_FILE = "/content/test/data/android/geolocation.html";

    private InfoBarTestAnimationListener mListener;
    private EmbeddedTestServer mTestServer;

    /**
     * Waits till the geolocation JavaScript callback is called the specified number of times.
     */
    private class GeolocationUpdateWaiter extends EmptyTabObserver {
        private CallbackHelper mCallbackHelper;
        private int mExpectedCount;

        public GeolocationUpdateWaiter() {
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            String expectedTitle = "Count:" + mExpectedCount;
            if (getActivity().getActivityTab().getTitle().equals(expectedTitle)) {
                mCallbackHelper.notifyCalled();
            }
        }

        public void waitForNumUpdates(int numUpdates) throws Exception {
            mExpectedCount = numUpdates;
            mCallbackHelper.waitForCallback(0);
        }
    }

    public GeolocationTest() {
        super(ChromeActivity.class);
        mSkipCheckHttpServer = true;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);

        InfoBarContainer container =
                getActivity().getTabModelSelector().getCurrentTab().getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        container.setAnimationListener(mListener);

        LocationProviderFactory.setLocationProviderImpl(new MockLocationProvider());

        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    /**
     * Verify Geolocation creates an InfoBar and receives a mock location.
     * @throws Exception
     */
    @Smoke
    @MediumTest
    @Feature({"Location", "Main"})
    public void testGeolocationPlumbing() throws Exception {
        final String url = mTestServer.getURL(TEST_FILE);

        Tab tab = getActivity().getActivityTab();
        GeolocationUpdateWaiter updateWaiter = new GeolocationUpdateWaiter();
        tab.addObserver(updateWaiter);

        loadUrl(url);
        runJavaScriptCodeInCurrentTab("initiate_getCurrentPosition()");
        assertTrue("InfoBar not added.", mListener.addInfoBarAnimationFinished());
        assertTrue("OK button wasn't found", InfoBarUtil.clickPrimaryButton(getInfoBars().get(0)));
        updateWaiter.waitForNumUpdates(1);

        tab.removeObserver(updateWaiter);
    }

    /**
     * Verify Geolocation creates an InfoBar and receives multiple locations.
     * @throws Exception
     */
    @MediumTest
    @Feature({"Location"})
    public void testGeolocationWatch() throws Exception {
        final String url = mTestServer.getURL(TEST_FILE);

        Tab tab = getActivity().getActivityTab();
        GeolocationUpdateWaiter updateWaiter = new GeolocationUpdateWaiter();
        tab.addObserver(updateWaiter);

        loadUrl(url);
        runJavaScriptCodeInCurrentTab("initiate_watchPosition()");
        assertTrue("InfoBar not added.", mListener.addInfoBarAnimationFinished());
        assertTrue("OK button wasn't found", InfoBarUtil.clickPrimaryButton(getInfoBars().get(0)));
        updateWaiter.waitForNumUpdates(2);

        tab.removeObserver(updateWaiter);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
