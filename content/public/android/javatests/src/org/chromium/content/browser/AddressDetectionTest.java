// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;

/**
 * Test suite for geographical US address detection.
 */
public class AddressDetectionTest extends ContentDetectionTestBase {

    private static final String GEO_INTENT_PREFIX = "geo:0,0?q=";

    private boolean isExpectedGeoIntent(String intentUrl, String expectedContent) {
        if (intentUrl == null) return false;
        final String expectedUrl = GEO_INTENT_PREFIX + urlForContent(expectedContent);
        return intentUrl.equals(expectedUrl);
    }

    @MediumTest
    @Feature({"ContentDetection", "TabContents"})
    public void testMultipleAddressesInText() throws Throwable {
        startActivityWithTestUrl("content/content_detection/geo_address_multiple.html");
        assertWaitForPageScaleFactorMatch(1.0f);

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test1"),
                "1600 Amphitheatre Parkway Mountain View, CA 94043"));

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test2"),
                "76 Ninth Avenue 4th Floor New York, NY 10011"));
    }

    @MediumTest
    @Feature({"ContentDetection", "TabContents"})
    public void testSplitAddresses() throws Throwable {
        startActivityWithTestUrl("content/content_detection/geo_address_split.html");
        assertWaitForPageScaleFactorMatch(1.0f);

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test1"),
                "9606 North MoPac Expressway Suite 400 Austin, TX 78759"));

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test2"),
                "1818 Library Street Suite 400, VA 20190"));

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test3"),
                "1818 Library Street Suite 400, VA 20190"));

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test4"),
                "1818 Library Street Suite 400, VA 20190"));
    }

    @MediumTest
    @Feature({"ContentDetection", "TabContents"})
    public void testAddressLimits() throws Throwable {
        startActivityWithTestUrl("content/content_detection/geo_address_limits.html");
        assertWaitForPageScaleFactorMatch(1.0f);

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test1"),
                "2590 Pearl Street Suite 100 Boulder, CO 80302"));

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test2"),
                "6425 Penn Ave. Suite 700 Pittsburgh, PA 15206"));

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test3"),
                "34 Main St. Boston, MA 02118"));

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test4"),
                "1600 Amphitheatre Parkway Mountain View, CA 94043"));
    }

    @MediumTest
    @Feature({"ContentDetection", "TabContents"})
    public void testRealAddresses() throws Throwable {
        startActivityWithTestUrl("content/content_detection/geo_address_real.html");
        assertWaitForPageScaleFactorMatch(1.0f);

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test1"),
                "57th Street and Lake Shore Drive Chicago, IL 60637"));

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test2"),
                "57th Street and Lake Shore Drive Chicago, IL 60637"));

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test3"),
                "57th Street and Lake Shore Drive Chicago, IL 60637"));

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test4"),
                "79th Street, New York, NY, 10024-5192"));
    }

    @MediumTest
    @Feature({"ContentDetection", "TabContents"})
    public void testSpecialChars() throws Throwable {
        startActivityWithTestUrl("content/content_detection/geo_address_special_chars.html");
        assertWaitForPageScaleFactorMatch(1.0f);

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test1"),
                "100 34th Avenue , San Francisco, CA 94121"));

        assertTrue(isExpectedGeoIntent(scrollAndTapExpectingIntent("test2"),
                "100 34th Avenue San Francisco, CA 94121"));
    }
}
