// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import android.content.Context;
import android.location.Location;
import android.location.LocationManager;
import android.os.Build;
import android.os.SystemClock;
import android.test.InstrumentationTestCase;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.GeolocationInfo;
import org.chromium.content.browser.BrowserStartupController;

/**
 * Tests for GeolocationHeader and GeolocationTracker.
 */
public class GeolocationHeaderTest extends InstrumentationTestCase {

    private static final String SEARCH_URL_1 = "https://www.google.com/search?q=potatoes";
    private static final String SEARCH_URL_2 = "https://www.google.co.jp/webhp?#q=dinosaurs";

    @SmallTest
    @Feature({"Location"})
    @UiThreadTest
    public void testGeolocationHeader() throws ProcessInitException {
        Context targetContext = getInstrumentation().getTargetContext();
        BrowserStartupController.get(targetContext, LibraryProcessType.PROCESS_BROWSER)
                .startBrowserProcessesSync(false);

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
        assertNotNull(getHeaderWithPermissions(ContentSetting.ALLOW, ContentSetting.ALLOW));
        assertNotNull(getHeaderWithPermissions(ContentSetting.ALLOW, ContentSetting.DEFAULT));
        assertNotNull(getHeaderWithPermissions(ContentSetting.ALLOW, ContentSetting.BLOCK));
        assertNotNull(getHeaderWithPermissions(ContentSetting.DEFAULT, ContentSetting.ALLOW));
        assertNotNull(getHeaderWithPermissions(ContentSetting.DEFAULT, ContentSetting.DEFAULT));
        assertNull(getHeaderWithPermissions(ContentSetting.DEFAULT, ContentSetting.BLOCK));
        assertNull(getHeaderWithPermissions(ContentSetting.BLOCK, ContentSetting.ALLOW));
        assertNull(getHeaderWithPermissions(ContentSetting.BLOCK, ContentSetting.DEFAULT));
        assertNull(getHeaderWithPermissions(ContentSetting.BLOCK, ContentSetting.BLOCK));

        // X-Geo should be sent only with non-stale locations.
        long now = System.currentTimeMillis();
        long oneHour = 60 * 60 * 1000;
        long oneWeek = 7 * 24 * 60 * 60 * 1000;
        assertNotNull(getHeaderWithLocation(20.3, 155.8, now));
        assertNotNull(getHeaderWithLocation(20.3, 155.8, now - oneHour));
        assertNull(getHeaderWithLocation(20.3, 155.8, now - oneWeek));
        GeolocationTracker.setLocationForTesting(null);
        assertNullHeader(SEARCH_URL_1, false);
    }

    private String getHeaderWithPermissions(ContentSetting httpsPermission,
            ContentSetting httpPermission) {
        GeolocationInfo infoHttps = new GeolocationInfo("https://www.google.de", null, false);
        GeolocationInfo infoHttp = new GeolocationInfo("http://www.google.de", null, false);
        infoHttps.setContentSetting(httpsPermission);
        infoHttp.setContentSetting(httpPermission);
        return GeolocationHeader.getGeoHeader(getInstrumentation().getTargetContext(),
                "https://www.google.de/search?q=kartoffelsalat", false);
    }

    private String getHeaderWithLocation(double latitute, double longitude, long time) {
        setMockLocation(latitute, longitude, time);
        return GeolocationHeader.getGeoHeader(getInstrumentation().getTargetContext(),
                SEARCH_URL_1, false);
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

    private void assertNullHeader(String url, boolean isIncognito) {
        Context targetContext = getInstrumentation().getTargetContext();
        assertNull(GeolocationHeader.getGeoHeader(targetContext, url, isIncognito));
    }

    private void assertNonNullHeader(String url, boolean isIncognito) {
        Context targetContext = getInstrumentation().getTargetContext();
        assertNotNull(GeolocationHeader.getGeoHeader(targetContext, url, isIncognito));
    }
}
