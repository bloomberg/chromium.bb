// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import android.location.Location;
import android.location.LocationManager;
import android.os.Build;
import android.os.SystemClock;
import android.support.test.filters.SmallTest;
import android.util.Base64;

import com.google.protobuf.nano.MessageNano;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.GeolocationInfo;
import org.chromium.chrome.browser.preferences.website.WebsitePreferenceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.Locale;

/**
 * Tests for GeolocationHeader and GeolocationTracker.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class GeolocationHeaderTest {

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String SEARCH_URL_1 = "https://www.google.com/search?q=potatoes";
    private static final String SEARCH_URL_2 = "https://www.google.co.jp/webhp?#q=dinosaurs";
    private static final String DISABLE_FEATURES = "disable-features=";
    private static final String ENABLE_FEATURES = "enable-features=";
    private static final String XGEO_VISIBLE_NETWORKS_FEATURE = "XGEOVisibleNetworks";
    private static final String GOOGLE_BASE_URL_SWITCH = "google-base-url=https://www.google.com";
    private static final double LOCATION_LAT = 20.3;
    private static final double LOCATION_LONG = 155.8;
    private static final float LOCATION_ACCURACY = 20f;
    private static final boolean ENCODING_PROTO = true;
    private static final boolean ENCODING_ASCII = false;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @CommandLineFlags
            .Add({GOOGLE_BASE_URL_SWITCH, DISABLE_FEATURES + XGEO_VISIBLE_NETWORKS_FEATURE})
    public void testConsistentHeader() {
        long now = setMockLocationNow();

        // X-Geo should be sent for Google search results page URLs.
        assertNonNullHeader(SEARCH_URL_1, false, now, ENCODING_ASCII);

        // But only the current CCTLD.
        assertNullHeader(SEARCH_URL_2, false);

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
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @CommandLineFlags
            .Add({GOOGLE_BASE_URL_SWITCH, DISABLE_FEATURES + XGEO_VISIBLE_NETWORKS_FEATURE})
    public void testPermissionAndSetting() {
        long now = setMockLocationNow();

        // X-Geo shouldn't be sent when location is disallowed for the origin, or when the DSE
        // geolocation setting is off.
        checkHeaderWithPermissionAndSetting(ContentSetting.ALLOW, true, now, false);
        checkHeaderWithPermissionAndSetting(ContentSetting.DEFAULT, true, now, false);
        checkHeaderWithPermissionAndSetting(ContentSetting.DEFAULT, false, now, true);
        checkHeaderWithPermissionAndSetting(ContentSetting.BLOCK, false, now, true);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @CommandLineFlags.Add({DISABLE_FEATURES + XGEO_VISIBLE_NETWORKS_FEATURE})
    public void testAsciiEncoding() {
        long now = setMockLocationNow();

        // X-Geo should be sent for Google search results page URLs using ascii encoding.
        assertNonNullHeader(SEARCH_URL_1, false, now, ENCODING_ASCII);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @CommandLineFlags.Add({ENABLE_FEATURES + XGEO_VISIBLE_NETWORKS_FEATURE})
    public void testProtoEncoding() {
        long now = setMockLocationNow();

        // X-Geo should be sent for Google search results page URLs using proto encoding.
        assertNonNullHeader(SEARCH_URL_1, false, now, ENCODING_PROTO);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @CommandLineFlags.Add({DISABLE_FEATURES + XGEO_VISIBLE_NETWORKS_FEATURE})
    public void testGpsFallbackNotEnabled() {
        // Only GPS location, should not be sent when flag is off.
        long now = System.currentTimeMillis();
        Location gpsLocation = generateMockLocation(LocationManager.GPS_PROVIDER, now);
        GeolocationTracker.setLocationForTesting(null, gpsLocation);

        assertNullHeader(SEARCH_URL_1, false);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @CommandLineFlags.Add({ENABLE_FEATURES + XGEO_VISIBLE_NETWORKS_FEATURE})
    public void testGpsFallbackEnabled() {
        // Only GPS location, should be sent when flag is on.
        long now = System.currentTimeMillis();
        Location gpsLocation = generateMockLocation(LocationManager.GPS_PROVIDER, now);
        GeolocationTracker.setLocationForTesting(null, gpsLocation);

        assertNonNullHeader(SEARCH_URL_1, false, now, ENCODING_PROTO);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @CommandLineFlags.Add({ENABLE_FEATURES + XGEO_VISIBLE_NETWORKS_FEATURE})
    public void testGpsFallbackYounger() {
        long now = System.currentTimeMillis();
        // GPS location is younger.
        Location gpsLocation = generateMockLocation(LocationManager.GPS_PROVIDER, now + 100);
        // Network location is older
        Location netLocation = generateMockLocation(LocationManager.NETWORK_PROVIDER, now);
        GeolocationTracker.setLocationForTesting(netLocation, gpsLocation);

        // The younger (GPS) should be used.
        assertNonNullHeader(SEARCH_URL_1, false, now + 100, ENCODING_PROTO);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @CommandLineFlags.Add({ENABLE_FEATURES + XGEO_VISIBLE_NETWORKS_FEATURE})
    public void testGpsFallbackOlder() {
        long now = System.currentTimeMillis();
        // GPS location is older.
        Location gpsLocation = generateMockLocation(LocationManager.GPS_PROVIDER, now - 100);
        // Network location is older
        Location netLocation = generateMockLocation(LocationManager.NETWORK_PROVIDER, now);
        GeolocationTracker.setLocationForTesting(netLocation, gpsLocation);

        // The younger (Network) should be used.
        assertNonNullHeader(SEARCH_URL_1, false, now, ENCODING_PROTO);
    }

    private void checkHeaderWithPermissions(final ContentSetting httpsPermission,
            final long locationTime, final boolean shouldBeNull) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                GeolocationInfo infoHttps =
                        new GeolocationInfo("https://www.google.de", null, false);
                infoHttps.setContentSetting(httpsPermission);
                String header = GeolocationHeader.getGeoHeader(
                        "https://www.google.de/search?q=kartoffelsalat",
                        mActivityTestRule.getActivity().getActivityTab());
                assertHeaderState(header, locationTime, shouldBeNull);
            }
        });
    }

    private void checkHeaderWithPermissionAndSetting(final ContentSetting httpsPermission,
            final boolean settingValue, final long locationTime, final boolean shouldBeNull) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                GeolocationInfo infoHttps =
                        new GeolocationInfo(SEARCH_URL_1, null, false);
                infoHttps.setContentSetting(httpsPermission);
                WebsitePreferenceBridge.setDSEGeolocationSetting(settingValue);
                String header = GeolocationHeader.getGeoHeader(
                        SEARCH_URL_1, mActivityTestRule.getActivity().getActivityTab());
                assertHeaderState(header, locationTime, shouldBeNull);
            }
        });
    }

    private void checkHeaderWithLocation(final long locationTime, final boolean shouldBeNull) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                setMockLocation(locationTime);
                String header = GeolocationHeader.getGeoHeader(SEARCH_URL_1,
                        mActivityTestRule.getActivity().getActivityTab());
                assertHeaderState(header, locationTime, shouldBeNull);
            }
        });
    }

    private void assertHeaderState(String header, long locationTime, boolean shouldBeNull) {
        if (shouldBeNull) {
            Assert.assertNull(header);
        } else {
            assertHeaderEquals(locationTime, ENCODING_ASCII, header);
        }
    }

    private long setMockLocationNow() {
        long now = System.currentTimeMillis();
        setMockLocation(now);
        return now;
    }

    private Location generateMockLocation(String provider, long time) {
        Location location = new Location(provider);
        location.setLatitude(LOCATION_LAT);
        location.setLongitude(LOCATION_LONG);
        location.setAccuracy(LOCATION_ACCURACY);
        location.setTime(time);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            location.setElapsedRealtimeNanos(SystemClock.elapsedRealtimeNanos()
                    + 1000000 * (time - System.currentTimeMillis()));
        }
        return location;
    }

    private void setMockLocation(long time) {
        Location location = generateMockLocation(LocationManager.NETWORK_PROVIDER, time);
        GeolocationTracker.setLocationForTesting(location, null);
    }

    private void assertNullHeader(final String url, final boolean isIncognito) {
        try {
            final Tab tab = mActivityTestRule.loadUrlInNewTab("about:blank", isIncognito);
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    Assert.assertNull(GeolocationHeader.getGeoHeader(url, tab));
                }
            });
        } catch (InterruptedException e) {
            Assert.fail(e.getMessage());
        }
    }

    private void assertNonNullHeader(final String url, final boolean isIncognito,
            final long locationTime, final boolean encodingType) {
        try {
            final Tab tab = mActivityTestRule.loadUrlInNewTab("about:blank", isIncognito);
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    assertHeaderEquals(
                            locationTime, encodingType, GeolocationHeader.getGeoHeader(url, tab));
                }
            });
        } catch (InterruptedException e) {
            Assert.fail(e.getMessage());
        }
    }

    private void assertHeaderEquals(long locationTime, boolean encodingType, String header) {
        long timestamp = locationTime * 1000;
        // Latitude times 1e7.
        int latitudeE7 = (int) (LOCATION_LAT * 10000000);
        // Longitude times 1e7.
        int longitudeE7 = (int) (LOCATION_LONG * 10000000);
        // Radius of 68% accuracy in mm.
        int radius = (int) (LOCATION_ACCURACY * 1000);

        String expectedHeader;
        if (encodingType == ENCODING_PROTO) {
            // Create a LatLng for the coordinates.
            PartnerLocationDescriptor.LatLng latlng = new PartnerLocationDescriptor.LatLng();
            latlng.latitudeE7 = latitudeE7;
            latlng.longitudeE7 = longitudeE7;

            // Populate a LocationDescriptor with the LatLng.
            PartnerLocationDescriptor.LocationDescriptor locationDescriptor =
                    new PartnerLocationDescriptor.LocationDescriptor();
            locationDescriptor.latlng = latlng;
            // Include role, producer, timestamp and radius.
            locationDescriptor.role = PartnerLocationDescriptor.CURRENT_LOCATION;
            locationDescriptor.producer = PartnerLocationDescriptor.DEVICE_LOCATION;
            locationDescriptor.timestamp = timestamp;
            locationDescriptor.radius = (float) radius;

            String locationProto = Base64.encodeToString(
                    MessageNano.toByteArray(locationDescriptor), Base64.NO_WRAP | Base64.URL_SAFE);
            expectedHeader = "X-Geo: w " + locationProto;
        } else {
            Assert.assertEquals(ENCODING_ASCII, encodingType);
            String locationAscii = String.format(Locale.US,
                    "role:1 producer:12 timestamp:%d latlng{latitude_e7:%d longitude_e7:%d} "
                            + "radius:%d",
                    timestamp, latitudeE7, longitudeE7, radius);
            expectedHeader = "X-Geo: a "
                    + new String(Base64.encode(locationAscii.getBytes(), Base64.NO_WRAP));
        }
        Assert.assertEquals(expectedHeader, header);
    }
}
