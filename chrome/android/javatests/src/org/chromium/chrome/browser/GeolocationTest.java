// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.location.Location;
import android.test.FlakyTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.LocationProviderAdapter;
import org.chromium.content.browser.LocationProviderFactory;
import org.chromium.content.browser.LocationProviderFactory.LocationProvider;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.util.List;

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

    private InfoBarTestAnimationListener mListener;

    public GeolocationTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        InfoBarContainer container =
                getActivity().getTabModelSelector().getCurrentTab().getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        container.setAnimationListener(mListener);

        LocationProviderFactory.setLocationProviderImpl(new LocationProvider() {
            private boolean mIsRunning;

            @Override
            public boolean isRunning() {
                return mIsRunning;
            }

            @Override
            public void start(boolean gpsEnabled) {
                mIsRunning = true;
            }

            @Override
            public void stop() {
                mIsRunning = false;
            }
        });
    }

    /**
     * Verify Geolocation creates an InfoBar and receives a mock location.
     *
     * Fails frequently.
     * Bug 141518
     * @Smoke
     * @MediumTest
     * @Feature({"Location", "Main"})
     *
     * @throws Exception
     */
    @FlakyTest
    public void testGeolocationPlumbing() throws Exception {
        final String url = TestHttpServerClient.getUrl(
                "chrome/test/data/geolocation/geolocation_on_load.html");

        Tab tab = getActivity().getActivityTab();
        final CallbackHelper loadCallback = new CallbackHelper();
        TabObserver observer = new EmptyTabObserver() {
            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                // If the device has a cached non-mock location, we won't get back our
                // lat/long, so checking that it has "#pass" is sufficient.
                if (tab.getUrl().startsWith(url + "#pass|")) {
                    loadCallback.notifyCalled();
                }
            }
        };
        tab.addObserver(observer);
        loadUrl(url);
        assertTrue("InfoBar not added.", mListener.addInfoBarAnimationFinished());
        List<InfoBar> infoBars = getActivity().getActivityTab().getInfoBarContainer().getInfoBars();
        assertTrue("OK button wasn't found", InfoBarUtil.clickPrimaryButton(infoBars.get(0)));

        sendLocation(createMockLocation(LATITUDE, LONGITUDE, ACCURACY));
        loadCallback.waitForCallback(0);
        tab.removeObserver(observer);
    }

    /**
     * Verify Geolocation creates an InfoBar and receives multiple locations.
     *
     * Bug 141518
     * @MediumTest
     * @Feature({"Location"})
     *
     * @throws Exception
     */
    @FlakyTest
    public void testGeolocationWatch() throws Exception {
        final String url = TestHttpServerClient.getUrl(
                "chrome/test/data/geolocation/geolocation_on_load.html");

        Tab tab = getActivity().getActivityTab();
        final CallbackHelper loadCallback0 = new CallbackHelper();
        TabObserver observer = new EmptyTabObserver() {
            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                // If the device has a cached non-mock location, we won't get back our
                // lat/long, so checking that it has "#pass" is sufficient.
                if (tab.getUrl().startsWith(url + "#pass|0|")) {
                    loadCallback0.notifyCalled();
                }
            }
        };
        tab.addObserver(observer);
        loadUrl(url);
        assertTrue("InfoBar not added.", mListener.addInfoBarAnimationFinished());

        List<InfoBar> infoBars = getActivity().getActivityTab().getInfoBarContainer().getInfoBars();
        assertTrue("OK button wasn't found", InfoBarUtil.clickPrimaryButton(infoBars.get(0)));

        sendLocation(createMockLocation(LATITUDE, LONGITUDE, ACCURACY));

        loadCallback0.waitForCallback(0);
        tab.removeObserver(observer);

        // Send another location: it'll update the URL again, so increase the count.
        final CallbackHelper loadCallback1 = new CallbackHelper();
        observer = new EmptyTabObserver() {
            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                // If the device has a cached non-mock location, we won't get back our
                // lat/long, so checking that it has "#pass" is sufficient.
                if (tab.getUrl().startsWith(url + "#pass|1|")) {
                    loadCallback1.notifyCalled();
                }
            }
        };
        tab.addObserver(observer);
        sendLocation(createMockLocation(LATITUDE + 1, LONGITUDE - 1, ACCURACY - 1));
        loadCallback1.waitForCallback(0);
        tab.removeObserver(observer);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    private Location createMockLocation(double latitude, double longitude, float accuracy) {
        Location mockLocation = new Location(LOCATION_PROVIDER_MOCK);
        mockLocation.setLatitude(latitude);
        mockLocation.setLongitude(longitude);
        mockLocation.setAccuracy(accuracy);
        mockLocation.setTime(System.currentTimeMillis());
        return mockLocation;
    }

    private void sendLocation(final Location location) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                LocationProviderAdapter.newLocationAvailable(
                        location.getLatitude(), location.getLongitude(),
                        location.getTime() / 1000.0,
                        location.hasAltitude(), location.getAltitude(),
                        location.hasAccuracy(), location.getAccuracy(),
                        location.hasBearing(), location.getBearing(),
                        location.hasSpeed(), location.getSpeed());
            }
        });
    }
}
