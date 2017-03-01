// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.res.Resources;
import android.text.TextUtils;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowResources;

import org.chromium.base.ContextUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.chrome.R;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Locale;

/**
 * Tests for {@link PwsClientImpl}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PwsClientImplTest {
    private static final String ACCEPT_LANGUAGES = "en-US,en";
    PwsClientImpl mPwsClientImpl;

    /**
     * Robolectric shadow to mock out calls to {@link Resources#getString}.
     */
    @Implements(Resources.class)
    public static class AcceptLanguageShadowResources extends ShadowResources {
        public static final Resources sResources = mock(Resources.class);

        @Override
        @Implementation
        public CharSequence getText(int id) {
            return sResources.getText(id);
        }
    }

    @Before
    public void setUp() throws Exception {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);
        mPwsClientImpl = new PwsClientImpl();
    }

    @Test
    public void testUserAgentNonEmpty() {
        assertFalse(TextUtils.isEmpty(mPwsClientImpl.getUserAgent()));
    }

    @Test
    @Config(shadows = AcceptLanguageShadowResources.class)
    public void testLanguageTagIsIncludedInAcceptLanguageHeader() {
        when(AcceptLanguageShadowResources.sResources.getText(R.string.accept_languages))
                .thenReturn(ACCEPT_LANGUAGES);
        String defaultLocaleString = LocaleUtils.getDefaultLocaleString();
        String[] languageTags = defaultLocaleString.split(",");

        // Ensure Accept-Language contains the full language tag.
        String acceptLanguage = mPwsClientImpl.updateAcceptLanguage();
        for (String tag : languageTags) {
            assertTrue(acceptLanguage.contains(tag));
            // Ensure Accept-Language also contains the language code by itself.
            String languageCode;
            if (tag.length() == 2 || tag.length() == 3) {
                languageCode = tag;
            } else if (tag.charAt(2) == '-') {
                languageCode = tag.substring(0, 2);
            } else { // length of the language code is 3.
                languageCode = tag.substring(0, 3);
            }
            assertTrue(acceptLanguage.startsWith(languageCode + ",")
                    || acceptLanguage.contains(languageCode + ";")
                    || acceptLanguage.equals(languageCode));
        }
    }

    @Test
    public void testLanguageTagIsPrepended() {
        Locale locale = new Locale("en", "GB");
        String defaultLocale = LocaleUtils.toLanguageTag(locale);
        String languageList = "fr-CA,fr-FR,fr";

        // Should prepend the language tag "en-GB" as well as the language code "en".
        String languageListWithTag =
                PwsClientImpl.prependToAcceptLanguagesIfNecessary(defaultLocale, languageList);
        assertEquals("en-GB,en,fr-CA,fr-FR,fr", languageListWithTag);
    }

    @Test
    public void testLanguageOnlyTagIsPrepended() {
        Locale locale = new Locale("mas");
        String defaultLocale = LocaleUtils.toLanguageTag(locale);
        String languageList = "fr-CA,fr-FR,fr";

        // Should prepend the language code only language tag "aaa".
        String languageListWithTag =
                PwsClientImpl.prependToAcceptLanguagesIfNecessary(defaultLocale, languageList);
        assertEquals("mas,fr-CA,fr-FR,fr", languageListWithTag);
    }

    @Test
    public void testSpecialLengthCountryCodeIsPrepended() {
        Locale locale = new Locale("es", "005");
        String defaultLocale = LocaleUtils.toLanguageTag(locale);
        String languageList = "fr-CA,fr-FR,fr";

        // Should prepend the language tag "aa-AAA" as well as the language code "aa".
        String languageListWithTag =
                PwsClientImpl.prependToAcceptLanguagesIfNecessary(defaultLocale, languageList);
        assertEquals("es-005,es,fr-CA,fr-FR,fr", languageListWithTag);
    }

    @Test
    public void testMultipleLanguageTagIsPrepended() {
        String locale = "jp-JP,is-IS";
        String languageList = "en-US,en";

        // Should prepend the language tag "aa-AA" as well as the language code "aa".
        String languageListWithTag =
                PwsClientImpl.prependToAcceptLanguagesIfNecessary(locale, languageList);
        assertEquals("jp-JP,jp,is-IS,is,en-US,en", languageListWithTag);

        // Make sure the language code is only inserted after the last languageTag that
        // contains that language.
        locale = "jp-JP,fr-CA,fr-FR";
        languageListWithTag =
                PwsClientImpl.prependToAcceptLanguagesIfNecessary(locale, languageList);
        assertEquals("jp-JP,jp,fr-CA,fr-FR,fr,en-US,en", languageListWithTag);
    }

    @Test
    public void testLanguageTagIsPrependedWhenListContainsLanguageCode() {
        Locale locale = new Locale("fr", "FR");
        String defaultLocale = LocaleUtils.toLanguageTag(locale);
        String languageList = "fr-CA,fr";

        // Should prepend the language tag "xx-XX" but not the language code "xx" as it's already
        // included at the end of the list.
        String languageListWithTag =
                PwsClientImpl.prependToAcceptLanguagesIfNecessary(defaultLocale, languageList);
        assertEquals("fr-FR,fr-CA,fr", languageListWithTag);
    }

    @Test
    public void testLanguageTagNotPrependedWhenUnnecessary() {
        Locale locale = new Locale("fr", "CA");
        String defaultLocale = LocaleUtils.toLanguageTag(locale);
        String languageList = "fr-CA,fr-FR,fr";

        // Language list should be unmodified since the tag is already present.
        String languageListWithTag =
                PwsClientImpl.prependToAcceptLanguagesIfNecessary(defaultLocale, languageList);
        assertEquals(languageList, languageListWithTag);
    }

    @Test
    public void testMultiLanguageTagNotPrependedWhenUnnecessary() {
        String locale = "fr-FR,is-IS";
        String languageList = "fr-FR,is-IS,fr,is";

        // Language list should be unmodified since the tag is already present. However, the order
        // changes because a language-code-only language tag is acceptable now.
        String languageListWithTag =
                PwsClientImpl.prependToAcceptLanguagesIfNecessary(locale, languageList);
        assertEquals(languageList, languageListWithTag);
    }

    @Test
    public void testAcceptLanguageQvalues() {
        String languageList = "en-US,en-GB,en,jp-JP,jp";

        // Should insert q-values for each item except the first which implicitly has q=1.0.
        String acceptLanguage = PwsClientImpl.generateAcceptLanguageHeader(languageList);
        assertEquals("en-US,en-GB;q=0.8,en;q=0.6,jp-JP;q=0.4,jp;q=0.2", acceptLanguage);

        // When there are six or more items, the q-value should not go below 0.2.
        languageList = "mas,es,en,jp,ch,fr";
        acceptLanguage = PwsClientImpl.generateAcceptLanguageHeader(languageList);
        assertEquals("mas,es;q=0.8,en;q=0.6,jp;q=0.4,ch;q=0.2,fr;q=0.2", acceptLanguage);
    }
}
