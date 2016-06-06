// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;

import java.util.Locale;

/**
 * Tests for the PwsClientImpl class.
 */
public class PwsClientImplTest extends InstrumentationTestCase {
    private PwsClientImpl mPwsClientImpl;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        mPwsClientImpl = new PwsClientImpl(context);
    }

    @SmallTest
    public void testUserAgentNonEmpty() {
        assertFalse(TextUtils.isEmpty(mPwsClientImpl.getUserAgent()));
    }

    @SmallTest
    public void testAcceptLanguageNonEmpty() {
        assertFalse(TextUtils.isEmpty(mPwsClientImpl.getAcceptLanguage()));
    }

    @SmallTest
    public void testDefaultLocaleIsValid() {
        // Ensure the current locale is of the form xx_XX, otherwise it will not be prepended
        // correctly in prependToAcceptLanguagesIfNecessary.
        String defaultLocale = Locale.getDefault().toString();
        assertEquals(5, defaultLocale.length());
        assertEquals('_', defaultLocale.charAt(2));
    }

    @SmallTest
    public void testMakeLanguageTag() {
        assertEquals("en-GB", PwsClientImpl.makeLanguageTag("en", "GB"));
        assertEquals("fr-CA", PwsClientImpl.makeLanguageTag("fr", "CA"));
        assertEquals("zh-TW", PwsClientImpl.makeLanguageTag("zh", "TW"));
    }

    @SmallTest
    public void testLanguageTagSpecialCases() {
        // Java mostly follows ISO-639-1 and ICU, except for the following three.
        // See documentation on java.util.Locale constructor for more.
        assertEquals("he-XX", PwsClientImpl.makeLanguageTag("iw", "XX"));
        assertEquals("yi-XX", PwsClientImpl.makeLanguageTag("ji", "XX"));
        assertEquals("id-XX", PwsClientImpl.makeLanguageTag("in", "XX"));
    }

    @SmallTest
    public void testLanguageTagIsIncludedInAcceptLanguageHeader() {
        String defaultLocale = Locale.getDefault().toString();
        String languageCode = defaultLocale.substring(0, 2);
        String countryCode = defaultLocale.substring(3);
        String languageTag = PwsClientImpl.makeLanguageTag(languageCode, countryCode);

        // Ensure Accept-Language contains the full language tag.
        String acceptLanguage = mPwsClientImpl.getAcceptLanguage();
        assertTrue(acceptLanguage.contains(languageTag));

        // Ensure Accept-Language also contains the language code by itself. Include the separator
        // character so we don't match, for instance, the "en" in "en-US".
        assertTrue(acceptLanguage.startsWith(languageCode + ",")
                || acceptLanguage.contains(languageCode + ";")
                || acceptLanguage.equals(languageCode));
    }

    @SmallTest
    public void testLanguageTagIsPrepended() {
        String locale = new Locale("aa", "AA").toString();
        String languageList = "xx-XX,xx,xx-YY";

        // Should prepend the language tag "aa-AA" as well as the language code "aa".
        String languageListWithTag = PwsClientImpl.prependToAcceptLanguagesIfNecessary(locale,
                languageList);
        assertEquals("aa-AA,aa,xx-XX,xx,xx-YY", languageListWithTag);
    }

    @SmallTest
    public void testLanguageTagIsPrependedWhenListContainsLanguageCode() {
        String locale = new Locale("xx", "XX").toString();
        String languageList = "xx-YY,xx";

        // Should prepend the language tag "xx-XX" but not the language code "xx" as it's already
        // included at the end of the list.
        String languageListWithTag = PwsClientImpl.prependToAcceptLanguagesIfNecessary(locale,
                languageList);
        assertEquals("xx-XX,xx-YY,xx", languageListWithTag);

        // Test again with the language code "xx" in the middle of the list.
        languageList = "xx-YY,xx,xx-ZZ";
        languageListWithTag = PwsClientImpl.prependToAcceptLanguagesIfNecessary(locale,
                languageList);
        assertEquals("xx-XX,xx-YY,xx,xx-ZZ", languageListWithTag);
    }

    @SmallTest
    public void testInvalidLanguageTagNotPrepended() {
        String locale = "not_valid";
        String languageList = "xx-XX,xx";

        // Language list should be unmodified since the language tag is invalid.
        String languageListWithTag = PwsClientImpl.prependToAcceptLanguagesIfNecessary(locale,
                languageList);
        assertEquals(languageList, languageListWithTag);
    }

    @SmallTest
    public void testLanguageTagNotPrependedWhenUnnecessary() {
        String locale = new Locale("xx", "XX").toString();
        String languageList = "xx-XX,xx,xx-YY";

        // Language list should be unmodified since the tag is already present.
        String languageListWithTag = PwsClientImpl.prependToAcceptLanguagesIfNecessary(locale,
                languageList);
        assertEquals(languageList, languageListWithTag);
    }

    @SmallTest
    public void testAcceptLanguageQvalues() {
        String languageList = "xx-XX,xx,xx-YY,zz-ZZ,zz";

        // Should insert q-values for each item except the first which implicitly has q=1.0.
        String acceptLanguage = PwsClientImpl.generateAcceptLanguageHeader(languageList);
        assertEquals("xx-XX,xx;q=0.8,xx-YY;q=0.6,zz-ZZ;q=0.4,zz;q=0.2", acceptLanguage);

        // When there are six or more items, the q-value should not go below 0.2.
        languageList = "xx-XA,xx-XB,xx-XC,xx-XD,xx-XE,xx-XF";
        acceptLanguage = PwsClientImpl.generateAcceptLanguageHeader(languageList);
        assertEquals("xx-XA,xx-XB;q=0.8,xx-XC;q=0.6,xx-XD;q=0.4,xx-XE;q=0.2,xx-XF;q=0.2",
                acceptLanguage);
    }
}
