// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleCell;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleCell.RadioType;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleWifi;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Robolectric tests for {@link VisibleNetworks}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class VisibleNetworksTest {
    private static final String SSID1 = "ssid1";
    private static final String BSSID1 = "11:11:11:11:11:11";
    private static final Integer LEVEL1 = -1;
    private static final Long TIMESTAMP1 = 10L;
    private static final String SSID2 = "ssid2";
    private static final String BSSID2 = "11:11:11:11:11:12";
    private static final Integer LEVEL2 = -2;
    private static final Long TIMESTAMP2 = 20L;
    @RadioType
    private static final int[] RADIO_TYPES = {VisibleCell.UNKNOWN_RADIO_TYPE,
            VisibleCell.UNKNOWN_MISSING_LOCATION_PERMISSION_RADIO_TYPE, VisibleCell.CDMA_RADIO_TYPE,
            VisibleCell.GSM_RADIO_TYPE, VisibleCell.LTE_RADIO_TYPE, VisibleCell.WCDMA_RADIO_TYPE};
    private static final VisibleWifi VISIBLE_WIFI1 =
            VisibleWifi.create(SSID1, BSSID1, LEVEL1, TIMESTAMP1);
    private static final VisibleWifi VISIBLE_WIFI2 =
            VisibleWifi.create(SSID2, BSSID2, LEVEL2, TIMESTAMP2);
    private static VisibleCell.Builder sVisibleCellCommunBuilder =
            VisibleCell.builder(VisibleCell.GSM_RADIO_TYPE)
                    .setCellId(10)
                    .setLocationAreaCode(11)
                    .setMobileCountryCode(12)
                    .setMobileNetworkCode(13);
    private static final VisibleCell VISIBLE_CELL1 =
            sVisibleCellCommunBuilder.setTimestamp(10L).build();
    private static final VisibleCell VISIBLE_CELL1_DIFFERENT_TIMESTAMP =
            sVisibleCellCommunBuilder.setTimestamp(20L).build();
    private static final VisibleCell VISIBLE_CELL2 = VisibleCell.builder(VisibleCell.GSM_RADIO_TYPE)
                                                             .setCellId(30)
                                                             .setLocationAreaCode(31)
                                                             .setMobileCountryCode(32)
                                                             .setMobileNetworkCode(33)
                                                             .setTimestamp(30L)
                                                             .build();
    private static Set<VisibleCell> sAllVisibleCells =
            new HashSet<VisibleCell>(Arrays.asList(VISIBLE_CELL1, VISIBLE_CELL2));
    private static Set<VisibleCell> sAllVisibleCells2 = new HashSet<VisibleCell>(
            Arrays.asList(VISIBLE_CELL1, VISIBLE_CELL2, VISIBLE_CELL1_DIFFERENT_TIMESTAMP));
    private static Set<VisibleWifi> sAllVisibleWifis =
            new HashSet<VisibleWifi>(Arrays.asList(VISIBLE_WIFI1, VISIBLE_WIFI2));

    private static final VisibleNetworks VISIBLE_NETWORKS1 = VisibleNetworks.create(
            VISIBLE_WIFI1, VISIBLE_CELL1, sAllVisibleWifis, sAllVisibleCells);

    private static final VisibleNetworks VISIBLE_NETWORKS2 = VisibleNetworks.create(
            VISIBLE_WIFI2, VISIBLE_CELL2, sAllVisibleWifis, sAllVisibleCells2);

    @Test
    public void testVisibleWifiCreate() {
        VisibleWifi visibleWifi = VisibleWifi.create(SSID1, BSSID1, LEVEL1, TIMESTAMP1);
        assertEquals(SSID1, visibleWifi.ssid());
        assertEquals(BSSID1, visibleWifi.bssid());
        assertEquals(LEVEL1, visibleWifi.level());
        assertEquals(TIMESTAMP1, visibleWifi.timestampMs());
    }

    @Test
    public void testVisibleWifiEquals() {
        VisibleWifi copyOfVisibleWifi1 = VisibleWifi.create(VISIBLE_WIFI1.ssid(),
                VISIBLE_WIFI1.bssid(), VISIBLE_WIFI1.level(), VISIBLE_WIFI1.timestampMs());

        assertEquals(VISIBLE_WIFI1, copyOfVisibleWifi1);
        assertNotEquals(VISIBLE_WIFI1, VISIBLE_WIFI2);
    }

    @Test
    public void testVisibleWifiEqualsDifferentLevelAndTimestamp() {
        VisibleWifi visibleWifi3 = VisibleWifi.create(SSID2, BSSID2, LEVEL1, TIMESTAMP1);

        // visibleWifi3 has the same ssid and bssid as VISIBLE_WIFI2 but different level and
        // timestamp. The level and timestamp are excluded from the VisibleWifi equality checks.
        assertEquals(visibleWifi3, VISIBLE_WIFI2);
    }

    @Test
    public void testVisibleWifiHash() {
        VisibleWifi copyOfVisibleWifi1 = VisibleWifi.create(VISIBLE_WIFI1.ssid(),
                VISIBLE_WIFI1.bssid(), VISIBLE_WIFI1.level(), VISIBLE_WIFI1.timestampMs());

        assertEquals(VISIBLE_WIFI1.hashCode(), copyOfVisibleWifi1.hashCode());
        assertNotEquals(VISIBLE_WIFI1.hashCode(), VISIBLE_WIFI2.hashCode());
    }

    @Test
    public void testVisibleWifiHashDifferentLevelAndTimestamp() {
        VisibleWifi visibleWifi3 = VisibleWifi.create(SSID2, BSSID2, LEVEL1, TIMESTAMP1);
        // visibleWifi3 has the same ssid and bssid as VISIBLE_WIFI2 but different level and
        // timestamp. The level and timestamp are excluded from the VisibleWifi hash function.
        assertEquals(VISIBLE_WIFI2.hashCode(), visibleWifi3.hashCode());
    }

    @Test
    public void testVisibleCellBuilder() {
        for (@RadioType int radioType : RADIO_TYPES) {
            VisibleCell visibleCell = VisibleCell.builder(radioType).build();
            assertEquals(radioType, visibleCell.radioType());
        }
    }

    @Test
    public void testVisibleCellEquals() {
        VisibleCell copyOfVisibleCell1 =
                VisibleCell.builder(VISIBLE_CELL1.radioType())
                        .setCellId(VISIBLE_CELL1.cellId())
                        .setLocationAreaCode(VISIBLE_CELL1.locationAreaCode())
                        .setMobileCountryCode(VISIBLE_CELL1.mobileCountryCode())
                        .setMobileNetworkCode(VISIBLE_CELL1.mobileNetworkCode())
                        .setTimestamp(VISIBLE_CELL1.timestampMs())
                        .build();
        assertNotEquals(VISIBLE_CELL2, VISIBLE_CELL1);
        assertEquals(VISIBLE_CELL1, copyOfVisibleCell1);
    }

    @Test
    public void testVisibleCellEqualsDifferentTimestamp() {
        // The timestamp is not included in the VisibleCell equality checks.
        assertEquals(VISIBLE_CELL1, VISIBLE_CELL1_DIFFERENT_TIMESTAMP);
    }

    @Test
    public void testVisibleCellHash() {
        VisibleCell copyOfVisibleCell1 =
                VisibleCell.builder(VISIBLE_CELL1.radioType())
                        .setCellId(VISIBLE_CELL1.cellId())
                        .setLocationAreaCode(VISIBLE_CELL1.locationAreaCode())
                        .setMobileCountryCode(VISIBLE_CELL1.mobileCountryCode())
                        .setMobileNetworkCode(VISIBLE_CELL1.mobileNetworkCode())
                        .setTimestamp(VISIBLE_CELL1.timestampMs())
                        .build();

        assertEquals(VISIBLE_CELL1.hashCode(), copyOfVisibleCell1.hashCode());
        assertNotEquals(VISIBLE_CELL2.hashCode(), VISIBLE_CELL1.hashCode());
    }

    @Test
    public void testVisibleCellHashDifferentTimestamp() {
        // The timestamp is not included in the VisibleCell hash function.
        assertEquals(VISIBLE_CELL1.hashCode(), VISIBLE_CELL1_DIFFERENT_TIMESTAMP.hashCode());
    }

    @Test
    public void testVisibleNetworksCreate() {
        Set<VisibleCell> expectedVisibleCells =
                new HashSet<VisibleCell>(Arrays.asList(VISIBLE_CELL1, VISIBLE_CELL2));
        Set<VisibleWifi> expectedVisibleWifis =
                new HashSet<VisibleWifi>(Arrays.asList(VISIBLE_WIFI1, VISIBLE_WIFI2));
        VisibleNetworks visibleNetworks = VisibleNetworks.create(
                VISIBLE_WIFI1, VISIBLE_CELL1, expectedVisibleWifis, expectedVisibleCells);
        assertEquals(VISIBLE_WIFI1, visibleNetworks.connectedWifi());
        assertEquals(VISIBLE_CELL1, visibleNetworks.connectedCell());
        assertEquals(expectedVisibleWifis, visibleNetworks.allVisibleWifis());
        assertEquals(expectedVisibleCells, visibleNetworks.allVisibleCells());
    }

    @Test
    public void testVisibleNetworksEquals() {
        VisibleNetworks copyOfVisibleNetworks1 = VisibleNetworks.create(
                VISIBLE_NETWORKS1.connectedWifi(), VISIBLE_NETWORKS1.connectedCell(),
                VISIBLE_NETWORKS1.allVisibleWifis(), VISIBLE_NETWORKS1.allVisibleCells());

        assertEquals(VISIBLE_NETWORKS1, copyOfVisibleNetworks1);
        assertNotEquals(VISIBLE_NETWORKS1, VISIBLE_NETWORKS2);
    }

    @Test
    public void testVisibleNetworksHash() {
        VisibleNetworks copyOfVisibleNetworks1 = VisibleNetworks.create(
                VISIBLE_NETWORKS1.connectedWifi(), VISIBLE_NETWORKS1.connectedCell(),
                VISIBLE_NETWORKS1.allVisibleWifis(), VISIBLE_NETWORKS1.allVisibleCells());

        assertEquals(VISIBLE_NETWORKS1.hashCode(), copyOfVisibleNetworks1.hashCode());
        assertNotEquals(VISIBLE_NETWORKS1.hashCode(), VISIBLE_NETWORKS2.hashCode());
    }

    @Test
    public void testVisibleNetworksIsEmpty() {
        VisibleNetworks visibleNetworks = VisibleNetworks.create(null, null, null, null);
        assertTrue(visibleNetworks.isEmpty());
        assertFalse(VISIBLE_NETWORKS1.isEmpty());
    }
}