// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.support.test.filters.SmallTest;
import android.test.AndroidTestCase;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.infobar.TranslateOptions;

import java.util.ArrayList;

/**
 * Test for TranslateOptions.
 */
public class TranslateOptionsTest extends AndroidTestCase {
    private static final boolean ALWAYS_TRANSLATE = true;
    private ArrayList<TranslateOptions.TranslateLanguagePair> mLanguages = null;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mLanguages = new ArrayList<TranslateOptions.TranslateLanguagePair>();
        mLanguages.add(new TranslateOptions.TranslateLanguagePair("en", "English"));
        mLanguages.add(new TranslateOptions.TranslateLanguagePair("es", "Spanish"));
        mLanguages.add(new TranslateOptions.TranslateLanguagePair("fr", "French"));
    }

    @SmallTest
    @Feature({"Translate"})
    public void testNoChanges() {
        TranslateOptions options =
                new TranslateOptions("en", "es", mLanguages, ALWAYS_TRANSLATE, false);
        assertEquals("English", options.sourceLanguageName());
        assertEquals("Spanish", options.targetLanguageName());
        assertEquals("en", options.sourceLanguageCode());
        assertEquals("es", options.targetLanguageCode());
        assertFalse(options.neverTranslateLanguageState());
        assertTrue(options.alwaysTranslateLanguageState());
        assertFalse(options.neverTranslateDomainState());
        assertFalse(options.optionsChanged());
    }

    @SmallTest
    @Feature({"Translate"})
    public void testBasicLanguageChanges() {
        TranslateOptions options =
                new TranslateOptions("en", "es", mLanguages, !ALWAYS_TRANSLATE, true);
        options.setTargetLanguage("fr");
        options.setSourceLanguage("en");
        assertEquals("English", options.sourceLanguageName());
        assertEquals("French", options.targetLanguageName());
        assertEquals("en", options.sourceLanguageCode());
        assertEquals("fr", options.targetLanguageCode());
        assertTrue(options.triggeredFromMenu());

        assertTrue(options.optionsChanged());

        // Switch back to the original
        options.setSourceLanguage("en");
        options.setTargetLanguage("es");
        assertFalse(options.optionsChanged());
    }

    @SmallTest
    @Feature({"Translate"})
    public void testInvalidLanguageChanges() {
        TranslateOptions options =
                new TranslateOptions("en", "es", mLanguages, ALWAYS_TRANSLATE, false);

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
        TranslateOptions options =
                new TranslateOptions("en", "es", mLanguages, !ALWAYS_TRANSLATE, false);
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
        TranslateOptions options =
                new TranslateOptions("en", "es", mLanguages, ALWAYS_TRANSLATE, false);

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
