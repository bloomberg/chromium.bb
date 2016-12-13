// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import android.location.Location;
import android.location.LocationManager;
import android.os.Build;
import android.os.SystemClock;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.GeolocationInfo;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;

/**
 * Tests for GeolocationHeader and GeolocationTracker.
 */
public class GeolocationHeaderTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String SEARCH_URL_1 = "https://www.google.com/search?q=potatoes";
    private static final String SEARCH_URL_2 = "https://www.google.co.jp/webhp?#q=dinosaurs";

    public GeolocationHeaderTest() {
        super(ChromeActivity.class);
    }

    @SmallTest
    @Feature({"Location"})
    public void testGeolocationHeader() throws ProcessInitException {
        setMockLocation(20.3, 155.8, System.currentTimeMillis());

        // X-Geo should be sent for Google search results page URLs.
        assertNonNullHeader(SEARCH_URL_1, false);
        assertNonNullHeader(SEARCH_URL_2, false);

        // X-Geo shouldn't be sent in incognito mode.
        assertNullHeader(SEARCH_URL_1, true);
        assertNullHeader(SEARCH_URL_2, true);

        // X-Geo shouldn't be sent with URLs that aren't the Google search results page.
        assertNullHeader("invalid$url", false);
        assertNullHeader("https://www.chrome.fr/", false);
        assertNullHeader("https://www.google.com/", false);

        // X-Geo shouldn't be sent over HTTP.
        assertNullHeader("http://www.google.com/search?q=potatoes", false);
        assertNullHeader("http://www.google.com/webhp?#q=dinosaurs", false);

        // X-Geo shouldn't be sent when location is disallowed for https origin.
        // If https origin doesn't have a location setting, fall back to value for http origin.
        checkHeaderWithPermissions(ContentSetting.ALLOW, ContentSetting.ALLOW, false);
        checkHeaderWithPermissions(ContentSetting.ALLOW, ContentSetting.DEFAULT, false);
        checkHeaderWithPermissions(ContentSetting.ALLOW, ContentSetting.BLOCK, false);
        checkHeaderWithPermissions(ContentSetting.DEFAULT, ContentSetting.ALLOW, false);
        checkHeaderWithPermissions(ContentSetting.DEFAULT, ContentSetting.DEFAULT, false);
        checkHeaderWithPermissions(ContentSetting.DEFAULT, ContentSetting.BLOCK, true);
        checkHeaderWithPermissions(ContentSetting.BLOCK, ContentSetting.ALLOW, true);
        checkHeaderWithPermissions(ContentSetting.BLOCK, ContentSetting.DEFAULT, true);
        checkHeaderWithPermissions(ContentSetting.BLOCK, ContentSetting.BLOCK, true);

        // X-Geo should be sent only with non-stale locations.
        long now = System.currentTimeMillis();
        long oneHour = 60 * 60 * 1000;
        long oneWeek = 7 * 24 * 60 * 60 * 1000;
        checkHeaderWithLocation(20.3, 155.8, now, false);
        checkHeaderWithLocation(20.3, 155.8, now - oneHour, false);
        checkHeaderWithLocation(20.3, 155.8, now - oneWeek, true);
        GeolocationTracker.setLocationForTesting(null);
        assertNullHeader(SEARCH_URL_1, false);
    }

    private void checkHeaderWithPermissions(final ContentSetting httpsPermission,
            final ContentSetting httpPermission, final boolean shouldBeNull) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                GeolocationInfo infoHttps =
                        new GeolocationInfo("https://www.google.de", null, false);
                GeolocationInfo infoHttp = new GeolocationInfo("http://www.google.de", null, false);
                infoHttps.setContentSetting(httpsPermission);
                infoHttp.setContentSetting(httpPermission);
                String header = GeolocationHeader.getGeoHeader(ContextUtils.getApplicationContext(),
                        "https://www.google.de/search?q=kartoffelsalat",
                        getActivity().getActivityTab());
                assertHeaderState(header, shouldBeNull);
            }
        });
    }

    private void checkHeaderWithLocation(final double latitute, final double longitude,
            final long time, final boolean shouldBeNull) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                setMockLocation(latitute, longitude, time);
                String header = GeolocationHeader.getGeoHeader(ContextUtils.getApplicationContext(),
                        SEARCH_URL_1, getActivity().getActivityTab());
                assertHeaderState(header, shouldBeNull);
            }
        });
    }

    private void assertHeaderState(String header, boolean shouldBeNull) {
        if (shouldBeNull) {
            assertNull(header);
        } else {
            assertNotNull(header);
        }
    }

    private void setMockLocation(double latitute, double longitude, long time) {
        final Location location = new Location(LocationManager.NETWORK_PROVIDER);
        location.setLatitude(latitute);
        location.setLongitude(longitude);
        location.setAccuracy(20f);
        location.setTime(time);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            location.setElapsedRealtimeNanos(SystemClock.elapsedRealtimeNanos()
                    + 1000000 * (time - System.currentTimeMillis()));
        }
        GeolocationTracker.setLocationForTesting(location);
    }

    private void assertNullHeader(final String url, final boolean isIncognito) {
        try {
            final Tab tab = loadUrlInNewTab("about:blank", isIncognito);
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    assertNull(GeolocationHeader.getGeoHeader(
                            ContextUtils.getApplicationContext(), url, tab));
                }
            });
        } catch (InterruptedException e) {
            fail(e.getMessage());
        }
    }

    private void assertNonNullHeader(final String url, final boolean isIncognito) {
        try {
            final Tab tab = loadUrlInNewTab("about:blank", isIncognito);
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    assertNotNull(GeolocationHeader.getGeoHeader(
                            ContextUtils.getApplicationContext(), url, tab));
                }
            });
        } catch (InterruptedException e) {
            fail(e.getMessage());
        }
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
