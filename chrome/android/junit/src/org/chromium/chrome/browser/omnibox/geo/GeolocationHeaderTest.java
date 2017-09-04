// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.location.Location;
import android.os.Build;
import android.os.SystemClock;
import android.util.Base64;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeaderTest.ShadowRecordHistogram;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeaderTest.ShadowUrlUtilities;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeaderTest.ShadowWebsitePreferenceBridge;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleCell;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleWifi;
import org.chromium.chrome.browser.preferences.website.WebsitePreferenceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Locale;

/**
 * Robolectric tests for {@link GeolocationHeader}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowUrlUtilities.class, ShadowRecordHistogram.class,
                ShadowWebsitePreferenceBridge.class})
public class GeolocationHeaderTest {
    private static final String SEARCH_URL = "https://www.google.com/search?q=potatoes";

    private static final double LOCATION_LAT = 20.3;
    private static final double LOCATION_LONG = 155.8;
    private static final float LOCATION_ACCURACY = 20f;
    private static final long LOCATION_TIME = 400;
    // Encoded location for LOCATION_LAT, LOCATION_LONG, LOCATION_ACCURACY and LOCATION_TIME.
    private static final String ENCODED_PROTO_LOCATION = "CAEQDBiAtRgqCg3AiBkMFYAx3Vw9AECcRg==";
    private static final String ENCODED_ASCII_LOCATION = "cm9sZToxIHByb2R1Y2VyOjEyIHRpbWVzdGFtcDo0M"
            + "DAwMDAgbGF0bG5ne2xhdGl0dWRlX2U3OjIwMzAwMDAwMCBsb25naXR1ZGVfZTc6MTU1ODAwMDAwMH0gcmFka"
            + "XVzOjIwMDAw";

    private static final VisibleWifi VISIBLE_WIFI1 =
            VisibleWifi.create("ssid1", "11:11:11:11:11:11", -1, 10L);
    private static final VisibleWifi VISIBLE_WIFI_NO_LEVEL =
            VisibleWifi.create("ssid1", "11:11:11:11:11:11", null, 10L);
    private static final VisibleWifi VISIBLE_WIFI2 =
            VisibleWifi.create("ssid2", "11:11:11:11:11:12", -10, 20L);
    private static final VisibleWifi VISIBLE_WIFI3 =
            VisibleWifi.create("ssid3", "11:11:11:11:11:13", -30, 30L);
    private static final VisibleWifi VISIBLE_WIFI_NOMAP =
            VisibleWifi.create("ssid1_nomap", "11:11:11:11:11:11", -1, 10L);
    private static final VisibleWifi VISIBLE_WIFI_OPTOUT =
            VisibleWifi.create("ssid1_optout", "11:11:11:11:11:11", -1, 10L);
    private static final VisibleCell VISIBLE_CELL1 =
            VisibleCell.builder(VisibleCell.CDMA_RADIO_TYPE)
                    .setCellId(10)
                    .setLocationAreaCode(11)
                    .setMobileCountryCode(12)
                    .setMobileNetworkCode(13)
                    .setTimestamp(10L)
                    .build();
    private static final VisibleCell VISIBLE_CELL2 = VisibleCell.builder(VisibleCell.GSM_RADIO_TYPE)
                                                             .setCellId(20)
                                                             .setLocationAreaCode(21)
                                                             .setMobileCountryCode(22)
                                                             .setMobileNetworkCode(23)
                                                             .setTimestamp(20L)
                                                             .build();
    // Encoded proto location for VISIBLE_WIFI1 connected, VISIBLE_WIFI3 not connected,
    // VISIBLE_CELL1 connected, VISIBLE_CELL2 not connected.
    private static final String ENCODED_PROTO_VISIBLE_NETWORKS =
            "CAEQDLoBJAoeChExMToxMToxMToxMToxMToxMRD___________8BGAEgCroBJAoeChExMToxMToxMToxMTox"
            + "MToxMxDi__________8BGAAgHroBEBIKCAMQChgLIAwoDRgBIAq6ARASCggBEBQYFSAWKBcYACAU";

    private static int sRefreshVisibleNetworksRequests = 0;
    private static int sRefreshLastKnownLocation = 0;

    @Rule
    public Features.Processor mFeatureProcessor = new Features.Processor();

    @Mock
    private Tab mTab;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        GeolocationTracker.setLocationAgeForTesting(null);
        GeolocationHeader.setLocationSourceForTesting(
                GeolocationHeader.LOCATION_SOURCE_HIGH_ACCURACY);
        GeolocationHeader.setAppPermissionGrantedForTesting(true);
        when(mTab.isIncognito()).thenReturn(false);
        sRefreshVisibleNetworksRequests = 0;
        sRefreshLastKnownLocation = 0;
    }

    @Test
    public void testEncodeAsciiLocation() throws ProcessInitException {
        Location location = generateMockLocation("should_not_matter", LOCATION_TIME);
        String encodedAsciiLocation = GeolocationHeader.encodeAsciiLocation(location);
        assertEquals(ENCODED_ASCII_LOCATION, encodedAsciiLocation);
    }

    @Test
    public void testEncodeProtoLocation() throws ProcessInitException {
        Location location = generateMockLocation("should_not_matter", LOCATION_TIME);
        String encodedProtoLocation = GeolocationHeader.encodeProtoLocation(location);
        assertEquals(ENCODED_PROTO_LOCATION, encodedProtoLocation);
    }

    @Test
    public void voidtestTrimVisibleNetworks() throws ProcessInitException {
        VisibleNetworks visibleNetworks =
                VisibleNetworks.create(VISIBLE_WIFI_NO_LEVEL, VISIBLE_CELL1,
                        new HashSet(Arrays.asList(VISIBLE_WIFI1, VISIBLE_WIFI2, VISIBLE_WIFI3)),
                        new HashSet(Arrays.asList(VISIBLE_CELL1, VISIBLE_CELL2)));

        // We expect trimming to replace connected Wifi (since it will have level), and select only
        // the visible wifi different from the connected one, with strongest level.
        VisibleNetworks expectedTrimmed = VisibleNetworks.create(VISIBLE_WIFI1, VISIBLE_CELL1,
                new HashSet(Arrays.asList(VISIBLE_WIFI3)),
                new HashSet(Arrays.asList(VISIBLE_CELL2)));

        VisibleNetworks trimmed = GeolocationHeader.trimVisibleNetworks(visibleNetworks);
        assertEquals(expectedTrimmed, trimmed);
    }

    @Test
    public void testTrimVisibleNetworksEmptyOrNull() throws ProcessInitException {
        VisibleNetworks visibleNetworks = VisibleNetworks.create(
                VisibleWifi.create("whatever", null, null, null),
                null, new HashSet(), new HashSet());
        assertNull(GeolocationHeader.trimVisibleNetworks(visibleNetworks));
        assertNull(GeolocationHeader.trimVisibleNetworks(null));
    }

    @Test
    public void testEncodeProtoVisibleNetworks() throws ProcessInitException {
        VisibleNetworks visibleNetworks = VisibleNetworks.create(VISIBLE_WIFI1, VISIBLE_CELL1,
                new HashSet(Arrays.asList(VISIBLE_WIFI3)),
                new HashSet(Arrays.asList(VISIBLE_CELL2)));
        String encodedProtoLocation = GeolocationHeader.encodeProtoVisibleNetworks(visibleNetworks);
        assertEquals(ENCODED_PROTO_VISIBLE_NETWORKS, encodedProtoLocation);
    }

    @Test
    public void testEncodeProtoVisibleNetworksEmptyOrNull() throws ProcessInitException {
        VisibleNetworks visibleNetworks =
                VisibleNetworks.create(null, null, new HashSet(), new HashSet());
        assertNull(GeolocationHeader.encodeProtoVisibleNetworks(visibleNetworks));
        assertNull(GeolocationHeader.encodeProtoVisibleNetworks(null));
    }

    @Test
    public void testEncodeProtoVisibleNetworksExcludeNoMapOrOptout() throws ProcessInitException {
        VisibleNetworks visibleNetworks = VisibleNetworks.create(VISIBLE_WIFI_NOMAP, null,
                new HashSet(Arrays.asList(VISIBLE_WIFI_OPTOUT)), new HashSet());
        String encodedProtoLocation = GeolocationHeader.encodeProtoVisibleNetworks(visibleNetworks);
        assertNull(encodedProtoLocation);
    }

    @Test
    @Features({ @Features.Register(ChromeFeatureList.XGEO_VISIBLE_NETWORKS) })
    public void testGetGeoHeaderFreshLocation() throws ProcessInitException {
        VisibleNetworks visibleNetworks = VisibleNetworks.create(VISIBLE_WIFI1, VISIBLE_CELL1,
                new HashSet(Arrays.asList(VISIBLE_WIFI3)),
                new HashSet(Arrays.asList(VISIBLE_CELL2)));
        VisibleNetworksTracker.setVisibleNetworksForTesting(visibleNetworks);
        Location location = generateMockLocation("should_not_matter", LOCATION_TIME);
        GeolocationTracker.setLocationForTesting(location, null);
        // 1 minute should be good enough and not require visible networks.
        GeolocationTracker.setLocationAgeForTesting(1 * 60 * 1000L);
        String header = GeolocationHeader.getGeoHeader(SEARCH_URL, mTab);
        assertEquals("X-Geo: w " + ENCODED_PROTO_LOCATION, header);
    }

    @Test
    @Features({ @Features.Register(ChromeFeatureList.XGEO_VISIBLE_NETWORKS) })
    public void testGetGeoHeaderLocationMissing() throws ProcessInitException {
        VisibleNetworks visibleNetworks = VisibleNetworks.create(VISIBLE_WIFI1, VISIBLE_CELL1,
                new HashSet(Arrays.asList(VISIBLE_WIFI3)),
                new HashSet(Arrays.asList(VISIBLE_CELL2)));
        VisibleNetworksTracker.setVisibleNetworksForTesting(visibleNetworks);
        GeolocationTracker.setLocationForTesting(null, null);
        String header = GeolocationHeader.getGeoHeader(SEARCH_URL, mTab);
        assertEquals("X-Geo: w " + ENCODED_PROTO_VISIBLE_NETWORKS, header);
    }

    @Test
    @Features({ @Features.Register(ChromeFeatureList.XGEO_VISIBLE_NETWORKS) })
    public void testGetGeoHeaderOldLocationHighAccuracy() throws ProcessInitException {
        GeolocationHeader.setLocationSourceForTesting(
                GeolocationHeader.LOCATION_SOURCE_HIGH_ACCURACY);
        // Visible networks should be included
        checkOldLocation(
                "X-Geo: w " + ENCODED_PROTO_LOCATION + " w " + ENCODED_PROTO_VISIBLE_NETWORKS);
    }

    @Test
    @Features({ @Features.Register(ChromeFeatureList.XGEO_VISIBLE_NETWORKS) })
    public void testGetGeoHeaderOldLocationBatterySaving() throws ProcessInitException {
        GeolocationHeader.setLocationSourceForTesting(
                GeolocationHeader.LOCATION_SOURCE_BATTERY_SAVING);
        checkOldLocation(
                "X-Geo: w " + ENCODED_PROTO_LOCATION + " w " + ENCODED_PROTO_VISIBLE_NETWORKS);
    }

    @Test
    @Features({ @Features.Register(ChromeFeatureList.XGEO_VISIBLE_NETWORKS) })
    public void testGetGeoHeaderOldLocationGpsOnly() throws ProcessInitException {
        GeolocationHeader.setLocationSourceForTesting(GeolocationHeader.LOCATION_SOURCE_GPS_ONLY);
        // In GPS only mode, networks should never be included.
        checkOldLocation("X-Geo: w " + ENCODED_PROTO_LOCATION);
    }

    @Test
    @Features({ @Features.Register(ChromeFeatureList.XGEO_VISIBLE_NETWORKS) })
    public void testGetGeoHeaderOldLocationLocationOff() throws ProcessInitException {
        GeolocationHeader.setLocationSourceForTesting(GeolocationHeader.LOCATION_SOURCE_MASTER_OFF);
        // If the master switch is off, networks should never be included (old location might).
        checkOldLocation("X-Geo: w " + ENCODED_PROTO_LOCATION);
    }

    @Test
    @Features({ @Features.Register(ChromeFeatureList.XGEO_VISIBLE_NETWORKS) })
    public void testGetGeoHeaderOldLocationAppPermissionDenied() throws ProcessInitException {
        GeolocationHeader.setLocationSourceForTesting(
                GeolocationHeader.LOCATION_SOURCE_HIGH_ACCURACY);
        GeolocationHeader.setAppPermissionGrantedForTesting(false);
        // Nothing should be included when app permission is missing.
        checkOldLocation(null);
    }

    @Test
    @Features({
            @Features.Register(value = ChromeFeatureList.XGEO_VISIBLE_NETWORKS, enabled = false)
    })
    public void testGetGeoHeaderOldLocationFeatureOff() throws ProcessInitException {
        long timestamp = LOCATION_TIME * 1000;
        int latitudeE7 = (int) (LOCATION_LAT * 10000000);
        int longitudeE7 = (int) (LOCATION_LONG * 10000000);
        int radius = (int) (LOCATION_ACCURACY * 1000);
        String locationAscii = String.format(Locale.US,
                "role:1 producer:12 timestamp:%d latlng{latitude_e7:%d longitude_e7:%d} "
                        + "radius:%d",
                timestamp, latitudeE7, longitudeE7, radius);
        String expectedHeader =
                "X-Geo: a " + new String(Base64.encode(locationAscii.getBytes(), Base64.NO_WRAP));
        checkOldLocation(expectedHeader);
    }

    @Test
    @Config(shadows = {ShadowVisibleNetworksTracker.class, ShadowGeolocationTracker.class})
    @Features(@Features.Register(value = ChromeFeatureList.XGEO_VISIBLE_NETWORKS, enabled = false))
    public void testPrimeLocationForGeoHeaderFeatureOff() throws ProcessInitException {
        GeolocationHeader.primeLocationForGeoHeader();
        assertEquals(1, sRefreshLastKnownLocation);
        assertEquals(0, sRefreshVisibleNetworksRequests);
    }

    @Test
    @Config(shadows = {ShadowVisibleNetworksTracker.class, ShadowGeolocationTracker.class})
    @Features(@Features.Register(ChromeFeatureList.XGEO_VISIBLE_NETWORKS))
    public void testPrimeLocationForGeoHeaderFeatureOn() throws ProcessInitException {
        GeolocationHeader.primeLocationForGeoHeader();
        assertEquals(1, sRefreshLastKnownLocation);
        assertEquals(1, sRefreshVisibleNetworksRequests);
    }

    @Test
    @Config(shadows = {ShadowVisibleNetworksTracker.class, ShadowGeolocationTracker.class})
    @Features(@Features.Register(ChromeFeatureList.XGEO_VISIBLE_NETWORKS))
    public void testPrimeLocationForGeoHeaderPermissionOff() throws ProcessInitException {
        GeolocationHeader.setAppPermissionGrantedForTesting(false);
        GeolocationHeader.primeLocationForGeoHeader();
        assertEquals(0, sRefreshLastKnownLocation);
        assertEquals(0, sRefreshVisibleNetworksRequests);
    }

    private void checkOldLocation(String expectedHeader) throws ProcessInitException {
        VisibleNetworks visibleNetworks = VisibleNetworks.create(VISIBLE_WIFI1, VISIBLE_CELL1,
                new HashSet(Arrays.asList(VISIBLE_WIFI3)),
                new HashSet(Arrays.asList(VISIBLE_CELL2)));
        VisibleNetworksTracker.setVisibleNetworksForTesting(visibleNetworks);
        Location location = generateMockLocation("should_not_matter", LOCATION_TIME);
        GeolocationTracker.setLocationForTesting(location, null);
        // 6 minutes should hit the age limit, but the feature is off.
        GeolocationTracker.setLocationAgeForTesting(6 * 60 * 1000L);
        String header = GeolocationHeader.getGeoHeader(SEARCH_URL, mTab);
        assertEquals(expectedHeader, header);
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

    /**
     * Shadow for UrlUtilities
     */
    @Implements(UrlUtilities.class)
    public static class ShadowUrlUtilities {
        @Implementation
        public static boolean nativeIsGoogleSearchUrl(String url) {
            return true;
        }
    }

    /**
     * Shadow for RecordHistogram
     */
    @Implements(RecordHistogram.class)
    public static class ShadowRecordHistogram {
        @Implementation
        public static void recordEnumeratedHistogram(String name, int sample, int boundary) {
            // Noop.
        }
    }

    /**
     * Shadow for WebsitePreferenceBridge
     */
    @Implements(WebsitePreferenceBridge.class)
    public static class ShadowWebsitePreferenceBridge {
        @Implementation
        public static boolean shouldUseDSEGeolocationSetting(String origin, boolean isIncognito) {
            return true;
        }

        @Implementation
        public static boolean getDSEGeolocationSetting() {
            return true;
        }
    }

    /**
     * Shadow for VisibleNetworksTracker
     */
    @Implements(VisibleNetworksTracker.class)
    public static class ShadowVisibleNetworksTracker {
        @Implementation
        public static void refreshVisibleNetworks(final Context context) {
            sRefreshVisibleNetworksRequests++;
        }
    }

    /**
     * Shadow for GeolocationTracker
     */
    @Implements(GeolocationTracker.class)
    public static class ShadowGeolocationTracker {
        @Implementation
        public static void refreshLastKnownLocation(Context context, long maxAge) {
            sRefreshLastKnownLocation++;
        }
    }
}
