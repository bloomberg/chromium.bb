// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.infobar.TranslateOptions;

/**
 * Test for TranslateOptions.
 */
public class TranslateOptionsTest extends AndroidTestCase {
    private static final String[] languages = {"English", "Spanish", "French"};
    private static final boolean ALWAYS_TRANSLATE = true;

    @SmallTest
    @Feature({"Translate"})
    public void testNoChanges() {
        TranslateOptions options = new TranslateOptions(0, 1, languages, ALWAYS_TRANSLATE, false);
        assertEquals("English", options.sourceLanguage());
        assertEquals("Spanish", options.targetLanguage());
        assertEquals(0, options.sourceLanguageIndex());
        assertEquals(1, options.targetLanguageIndex());
        assertFalse(options.neverTranslateLanguageState());
        assertTrue(options.alwaysTranslateLanguageState());
        assertFalse(options.neverTranslateDomainState());
        assertFalse(options.optionsChanged());
    }

    @SmallTest
    @Feature({"Translate"})
    public void testBasicLanguageChanges() {
        TranslateOptions options = new TranslateOptions(0, 1, languages, !ALWAYS_TRANSLATE, true);
        options.setTargetLanguage(2);
        options.setSourceLanguage(1);
        assertEquals("Spanish", options.sourceLanguage());
        assertEquals("French", options.targetLanguage());
        assertEquals(1, options.sourceLanguageIndex());
        assertEquals(2, options.targetLanguageIndex());
        assertTrue(options.triggeredFromMenu());
        assertTrue(options.optionsChanged());

        // Switch back to the original
        options.setSourceLanguage(0);
        options.setTargetLanguage(1);
        assertFalse(options.optionsChanged());
    }

    @SmallTest
    @Feature({"Translate"})
    public void testInvalidLanguageChanges() {
        TranslateOptions options = new TranslateOptions(0, 1, languages, ALWAYS_TRANSLATE, false);

        // Same target language as source
        assertFalse(options.setTargetLanguage(0));
        assertFalse(options.optionsChanged());

        // Target language out of range
        assertFalse(options.setTargetLanguage(23));
        assertFalse(options.optionsChanged());

        // Same source and target
        assertFalse(options.setSourceLanguage(1));
        assertFalse(options.optionsChanged());

        // Source language out of range
        assertFalse(options.setSourceLanguage(23));
        assertFalse(options.optionsChanged());
    }

    @SmallTest
    @Feature({"Translate"})
    public void testBasicOptionsChanges() {
        TranslateOptions options = new TranslateOptions(0, 1, languages, !ALWAYS_TRANSLATE, false);
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
        TranslateOptions options = new TranslateOptions(0, 1, languages, ALWAYS_TRANSLATE, false);

        // Never translate language should not work, but never translate domain should
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
