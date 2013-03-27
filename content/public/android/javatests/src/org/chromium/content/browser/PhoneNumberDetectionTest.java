// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.content.common.CommandLine;

/**
 * Test suite for phone number detection.
 */
public class PhoneNumberDetectionTest extends ContentDetectionTestBase {

    private static final String TELEPHONE_INTENT_PREFIX = "tel:";

    private boolean isExpectedTelephoneIntent(String intentUrl, String expectedContent) {
        if (intentUrl == null) return false;
        final String expectedUrl = TELEPHONE_INTENT_PREFIX + urlForContent(expectedContent);
        return intentUrl.equals(expectedUrl);
    }

    /**
     * Starts the content shell activity with the provided test URL and setting the local country
     * to the one provided by its 2-letter ISO code.
     * @param testUrl Test url to load.
     * @param countryIso 2-letter ISO country code. If set to null only international numbers
     *                   can be assumed to be supported.
     */
    private void startActivityWithTestUrlAndCountryIso(String testUrl, String countryIso)
            throws Throwable {
        final String[] cmdlineArgs = countryIso == null ? null : new String[] {
                "--" + CommandLine.NETWORK_COUNTRY_ISO + "=" + countryIso };
        startActivityWithTestUrlAndCommandLineArgs(testUrl, cmdlineArgs);
    }

    @LargeTest
    @Feature({"ContentDetection", "TabContents"})
    public void testInternationalNumberIntents() throws Throwable {
        startActivityWithTestUrl("content/content_detection/phone_international.html");
        assertWaitForPageScaleFactorMatch(1.0f);

        // US: +1 650-253-0000.
        String intentUrl = scrollAndTapExpectingIntent("US");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+16502530000"));

        // Australia: +61 2 9374 4000.
        intentUrl = scrollAndTapExpectingIntent("Australia");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+61293744000"));

        // China: +86-10-62503000.
        intentUrl = scrollAndTapExpectingIntent("China");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+861062503000"));

        // Hong Kong: +852-3923-5400.
        intentUrl = scrollAndTapExpectingIntent("Hong Kong");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+85239235400"));

        // India: +91-80-67218000.
        intentUrl = scrollAndTapExpectingIntent("India");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+918067218000"));

        // Japan: +81-3-6384-9000.
        intentUrl = scrollAndTapExpectingIntent("Japan");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+81363849000"));

        // Korea: +82-2-531-9000.
        intentUrl = scrollAndTapExpectingIntent("Korea");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+8225319000"));

        // Singapore: +65 6521-8000.
        intentUrl = scrollAndTapExpectingIntent("Singapore");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+6565218000"));

        // Taiwan: +886 2 8729 6000.
        intentUrl = scrollAndTapExpectingIntent("Taiwan");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+886287296000"));

        // Kenya: +254 20 360 1000.
        intentUrl = scrollAndTapExpectingIntent("Kenya");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+254203601000"));

        // France: +33 (0)1 42 68 53 00.
        intentUrl = scrollAndTapExpectingIntent("France");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+33142685300"));

        // Germany: +49 40-80-81-79-000.
        intentUrl = scrollAndTapExpectingIntent("Germany");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+4940808179000"));

        // Ireland: +353 (1) 436 1001.
        intentUrl = scrollAndTapExpectingIntent("Ireland");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+35314361001"));

        // Italy: +39 02-36618 300.
        intentUrl = scrollAndTapExpectingIntent("Italy");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+390236618300"));

        // Netherlands: +31 (0)20-5045-100.
        intentUrl = scrollAndTapExpectingIntent("Netherlands");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+31205045100"));

        // Norway: +47 22996288.
        intentUrl = scrollAndTapExpectingIntent("Norway");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+4722996288"));

        // Poland: +48 (12) 68 15 300.
        intentUrl = scrollAndTapExpectingIntent("Poland");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+48126815300"));

        // Russia: +7-495-644-1400.
        intentUrl = scrollAndTapExpectingIntent("Russia");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+74956441400"));

        // Spain: +34 91-748-6400.
        intentUrl = scrollAndTapExpectingIntent("Spain");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+34917486400"));

        // Switzerland: +41 44-668-1800.
        intentUrl = scrollAndTapExpectingIntent("Switzerland");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+41446681800"));

        // UK: +44 (0)20-7031-3000.
        intentUrl = scrollAndTapExpectingIntent("UK");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+442070313000"));

        // Canada: +1 514-670-8700.
        intentUrl = scrollAndTapExpectingIntent("Canada");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+15146708700"));

        // Argentina: +54-11-5530-3000.
        intentUrl = scrollAndTapExpectingIntent("Argentina");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+541155303000"));

        // Brazil: +55-31-2128-6800.
        intentUrl = scrollAndTapExpectingIntent("Brazil");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+553121286800"));

        // Mexico: +52 55-5342-8400.
        intentUrl = scrollAndTapExpectingIntent("Mexico");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+525553428400"));

        // Israel: +972-74-746-6245.
        intentUrl = scrollAndTapExpectingIntent("Israel");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+972747466245"));

        // UAE: +971 4 4509500.
        intentUrl = scrollAndTapExpectingIntent("UAE");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+97144509500"));
    }

    @MediumTest
    @Feature({"ContentDetection", "TabContents"})
    public void testLocalUSNumbers() throws Throwable {
        startActivityWithTestUrlAndCountryIso("content/content_detection/phone_local.html", "US");
        assertWaitForPageScaleFactorMatch(1.0f);

        // US_1: 1-888-433-5788.
        String intentUrl = scrollAndTapExpectingIntent("US_1");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+18884335788"));

        // US_2: 703-293-6299.
        intentUrl = scrollAndTapExpectingIntent("US_2");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+17032936299"));

        // US_3: (202) 456-2121.
        intentUrl = scrollAndTapExpectingIntent("US_3");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+12024562121"));

        // International numbers should still work.
        intentUrl = scrollAndTapExpectingIntent("International");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+31205045100"));
    }

    @MediumTest
    @Feature({"ContentDetection", "TabContents"})
    public void testLocalUKNumbers() throws Throwable {
        startActivityWithTestUrlAndCountryIso("content/content_detection/phone_local.html", "GB");
        assertWaitForPageScaleFactorMatch(1.0f);

        // GB_1: (0) 20 7323 8299.
        String intentUrl = scrollAndTapExpectingIntent("GB_1");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+442073238299"));

        // GB_2: 01227865330.
        intentUrl = scrollAndTapExpectingIntent("GB_2");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+441227865330"));

        // GB_3: 01963 824686.
        intentUrl = scrollAndTapExpectingIntent("GB_3");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+441963824686"));

        // International numbers should still work.
        intentUrl = scrollAndTapExpectingIntent("International");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+31205045100"));
    }

    @MediumTest
    @Feature({"ContentDetection", "TabContents"})
    public void testLocalFRNumbers() throws Throwable {
        startActivityWithTestUrlAndCountryIso("content/content_detection/phone_local.html", "FR");
        assertWaitForPageScaleFactorMatch(1.0f);

        // FR_1: 01 40 20 50 50.
        String intentUrl = scrollAndTapExpectingIntent("FR_1");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+33140205050"));

        // FR_2: 0326475534.
        intentUrl = scrollAndTapExpectingIntent("FR_2");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+33326475534"));

        // FR_3: (0) 237 211 992.
        intentUrl = scrollAndTapExpectingIntent("FR_3");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+33237211992"));

        // International numbers should still work.
        intentUrl = scrollAndTapExpectingIntent("International");
        assertTrue(isExpectedTelephoneIntent(intentUrl, "+31205045100"));
    }
}
