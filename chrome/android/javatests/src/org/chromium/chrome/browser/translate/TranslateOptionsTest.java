// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.support.test.filters.SmallTest;
import android.test.AndroidTestCase;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.infobar.TranslateOptions;

/**
 * Test for TranslateOptions.
 */
public class TranslateOptionsTest extends AndroidTestCase {
    private static final boolean ALWAYS_TRANSLATE = true;
    private static final String[] LANGUAGES = {"English", "Spanish", "French"};
    private static final String[] CODES = {"en", "es", "fr"};
    private static final int[] UMA_HASH_CODES = {10, 20, 30};

    @Override
    public void setUp() throws Exception {
        super.setUp();
    }

    @SmallTest
    @Feature({"Translate"})
    public void testNoChanges() {
        TranslateOptions options = TranslateOptions.create(
                "en", "es", LANGUAGES, CODES, ALWAYS_TRANSLATE, false, null);
        assertEquals("English", options.sourceLanguageName());
        assertEquals("Spanish", options.targetLanguageName());
        assertEquals("en", options.sourceLanguageCode());
        assertEquals("es", options.targetLanguageCode());
        assertFalse(options.neverTranslateLanguageState());
        assertTrue(options.alwaysTranslateLanguageState());
        assertFalse(options.neverTranslateDomainState());
        assertFalse(options.optionsChanged());
        assertNull(options.getUMAHashCodeFromCode("en"));
    }

    @SmallTest
    @Feature({"Translate"})
    public void testBasicLanguageChanges() {
        TranslateOptions options = TranslateOptions.create(
                "en", "es", LANGUAGES, CODES, !ALWAYS_TRANSLATE, true, UMA_HASH_CODES);
        options.setTargetLanguage("fr");
        options.setSourceLanguage("en");
        assertEquals("English", options.sourceLanguageName());
        assertEquals("French", options.targetLanguageName());
        assertEquals("en", options.sourceLanguageCode());
        assertEquals("fr", options.targetLanguageCode());
        assertTrue(options.triggeredFromMenu());
        assertEquals(Integer.valueOf(10), options.getUMAHashCodeFromCode("en"));
        assertEquals("English", options.getRepresentationFromCode("en"));

        assertTrue(options.optionsChanged());

        // Switch back to the original
        options.setSourceLanguage("en");
        options.setTargetLanguage("es");
        assertFalse(options.optionsChanged());
    }

    @SmallTest
    @Feature({"Translate"})
    public void testInvalidLanguageChanges() {
        TranslateOptions options = TranslateOptions.create(
                "en", "es", LANGUAGES, CODES, ALWAYS_TRANSLATE, false, null);

        // Same target language as source
        assertFalse(options.setTargetLanguage("en"));
        assertFalse(options.optionsChanged());

        // Target language does not exist
        assertFalse(options.setTargetLanguage("aaa"));
        assertFalse(options.optionsChanged());

        // Same source and target
        assertFalse(options.setSourceLanguage("es"));
        assertFalse(options.optionsChanged());

        // Source language does not exist
        assertFalse(options.setSourceLanguage("bbb"));
        assertFalse(options.optionsChanged());
    }

    @SmallTest
    @Feature({"Translate"})
    public void testBasicOptionsChanges() {
        TranslateOptions options = TranslateOptions.create(
                "en", "es", LANGUAGES, CODES, !ALWAYS_TRANSLATE, false, null);
        assertFalse(options.optionsChanged());
        options.toggleNeverTranslateDomainState(true);
        assertTrue(options.neverTranslateDomainState());
        assertFalse(options.alwaysTranslateLanguageState());
        assertFalse(options.neverTranslateLanguageState());
        assertTrue(options.optionsChanged());
        options.toggleNeverTranslateDomainState(false);
        assertFalse(options.neverTranslateDomainState());
        assertFalse(options.neverTranslateLanguageState());
        assertFalse(options.alwaysTranslateLanguageState());

        // We are back to the original state
        assertFalse(options.optionsChanged());
        options.toggleAlwaysTranslateLanguageState(true);
        assertFalse(options.neverTranslateDomainState());
        assertFalse(options.neverTranslateLanguageState());
        assertTrue(options.alwaysTranslateLanguageState());
        assertTrue(options.optionsChanged());
    }

    @SmallTest
    @Feature({"Translate"})
    public void testInvalidOptionsChanges() {
        TranslateOptions options = TranslateOptions.create(
                "en", "es", LANGUAGES, CODES, ALWAYS_TRANSLATE, false, null);

        // Never translate language should not work, but never translate domain
        // should
        assertFalse(options.toggleNeverTranslateLanguageState(true));
        assertTrue(options.toggleNeverTranslateDomainState(true));
        assertTrue(options.optionsChanged());

        assertTrue(options.toggleAlwaysTranslateLanguageState(false));

        // Never options are ok
        assertTrue(options.toggleNeverTranslateLanguageState(true));
        assertTrue(options.toggleNeverTranslateDomainState(true));

        // But always is not now
        assertFalse(options.toggleAlwaysTranslateLanguageState(true));
    }
}
