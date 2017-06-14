// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit tests for {@link LocationBarLayout} class.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class LocationBarLayoutTest {
    private static final boolean IS_SMALL_DEVICE = true;
    private static final boolean IS_OFFLINE_PAGE = true;
    private static final int[] SECURITY_LEVELS =
            new int[] {ConnectionSecurityLevel.NONE, ConnectionSecurityLevel.HTTP_SHOW_WARNING,
                    ConnectionSecurityLevel.SECURITY_WARNING, ConnectionSecurityLevel.DANGEROUS,
                    ConnectionSecurityLevel.SECURE, ConnectionSecurityLevel.EV_SECURE};

    @Mock
    private Tab mTab;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testGetSecurityLevel() {
        assertEquals(ConnectionSecurityLevel.NONE,
                LocationBarLayout.getSecurityLevel(null, !IS_OFFLINE_PAGE));
        assertEquals(ConnectionSecurityLevel.NONE,
                LocationBarLayout.getSecurityLevel(null, IS_OFFLINE_PAGE));
        assertEquals(ConnectionSecurityLevel.NONE,
                LocationBarLayout.getSecurityLevel(mTab, IS_OFFLINE_PAGE));

        for (int securityLevel : SECURITY_LEVELS) {
            doReturn(securityLevel).when(mTab).getSecurityLevel();
            assertEquals(securityLevel, LocationBarLayout.getSecurityLevel(mTab, !IS_OFFLINE_PAGE));
        }
        verify(mTab, times(SECURITY_LEVELS.length)).getSecurityLevel();
    }

    @Test
    public void testGetSecurityIconResource() {
        for (int securityLevel : SECURITY_LEVELS) {
            assertEquals(R.drawable.offline_pin_round,
                    LocationBarLayout.getSecurityIconResource(
                            securityLevel, IS_SMALL_DEVICE, IS_OFFLINE_PAGE));
            assertEquals(R.drawable.offline_pin_round,
                    LocationBarLayout.getSecurityIconResource(
                            securityLevel, !IS_SMALL_DEVICE, IS_OFFLINE_PAGE));
        }

        assertEquals(0,
                LocationBarLayout.getSecurityIconResource(
                        ConnectionSecurityLevel.NONE, IS_SMALL_DEVICE, !IS_OFFLINE_PAGE));
        assertEquals(R.drawable.omnibox_info,
                LocationBarLayout.getSecurityIconResource(
                        ConnectionSecurityLevel.NONE, !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE));

        assertEquals(R.drawable.omnibox_info,
                LocationBarLayout.getSecurityIconResource(ConnectionSecurityLevel.HTTP_SHOW_WARNING,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE));
        assertEquals(R.drawable.omnibox_info,
                LocationBarLayout.getSecurityIconResource(ConnectionSecurityLevel.HTTP_SHOW_WARNING,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE));

        assertEquals(R.drawable.omnibox_info,
                LocationBarLayout.getSecurityIconResource(ConnectionSecurityLevel.SECURITY_WARNING,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE));
        assertEquals(R.drawable.omnibox_info,
                LocationBarLayout.getSecurityIconResource(ConnectionSecurityLevel.SECURITY_WARNING,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE));

        assertEquals(R.drawable.omnibox_https_invalid,
                LocationBarLayout.getSecurityIconResource(
                        ConnectionSecurityLevel.DANGEROUS, IS_SMALL_DEVICE, !IS_OFFLINE_PAGE));
        assertEquals(R.drawable.omnibox_https_invalid,
                LocationBarLayout.getSecurityIconResource(
                        ConnectionSecurityLevel.DANGEROUS, !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE));

        assertEquals(R.drawable.omnibox_https_valid,
                LocationBarLayout.getSecurityIconResource(
                        ConnectionSecurityLevel.SECURE, IS_SMALL_DEVICE, !IS_OFFLINE_PAGE));
        assertEquals(R.drawable.omnibox_https_valid,
                LocationBarLayout.getSecurityIconResource(
                        ConnectionSecurityLevel.SECURE, !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE));

        assertEquals(R.drawable.omnibox_https_valid,
                LocationBarLayout.getSecurityIconResource(
                        ConnectionSecurityLevel.EV_SECURE, IS_SMALL_DEVICE, !IS_OFFLINE_PAGE));
        assertEquals(R.drawable.omnibox_https_valid,
                LocationBarLayout.getSecurityIconResource(
                        ConnectionSecurityLevel.EV_SECURE, !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE));
    }
}
