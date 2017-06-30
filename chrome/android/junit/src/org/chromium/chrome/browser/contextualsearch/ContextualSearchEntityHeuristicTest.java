// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Feature;

import java.util.Locale;

/**
 * Tests parts of the {@link ContextualSearchEntityHeuristic} class.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ContextualSearchEntityHeuristicTest {
    private static final String SAMPLE_TEXT =
            "Now Barack Obama, Michelle are not the best examples.  And Clinton is ambiguous.";
    private static final String UTF_8 = "UTF-8";

    private ContextualSearchContext mContext;
    private ContextualSearchEntityHeuristic mEntityHeuristic;

    @Before
    public void setup() {
        mContext = new ContextualSearchContextForTest();
    }

    private void setupInstanceToTest(Locale locale, int tapOffset) {
        mContext.setSurroundingText(UTF_8, SAMPLE_TEXT, tapOffset, tapOffset);
        mContext.setResolveProperties(locale.getCountry().toString(), true);
        mEntityHeuristic = ContextualSearchEntityHeuristic.testInstance(mContext, true);
    }

    private void setupTapInObama(Locale locale) {
        setupInstanceToTest(locale, "Now Barack Oba".length());
    }

    private void setupTapInEnglishStartOfBuffer() {
        setupInstanceToTest(Locale.US, 0);
    }

    private void setupTapInEnglishClinton() {
        setupInstanceToTest(Locale.US,
                "Now Barack Obama, Michelle are not the best examples.  And Clin".length());
    }

    private void setupTapInWordAfterComma() {
        setupInstanceToTest(Locale.US, "Now Barack Obama, Mich".length());
    }

    @Test
    @Feature({"ContextualSearch", "TapProperNounSuppression"})
    public void testTapInProperNounRecognized() {
        setupTapInObama(Locale.US);
        assertTrue(mEntityHeuristic.isProbablyEnglishProperNoun());
    }

    @Test
    @Feature({"ContextualSearch", "TapProperNounSuppression"})
    public void testTapInProperNounNotEnglishNotRecognized() {
        setupTapInObama(Locale.GERMANY);
        assertFalse(mEntityHeuristic.isProbablyEnglishProperNoun());
    }

    @Test
    @Feature({"ContextualSearch", "TapProperNounSuppression"})
    public void testTapInProperNounAfterPeriodNotRecognized() {
        setupTapInEnglishClinton();
        assertFalse(mEntityHeuristic.isProbablyEnglishProperNoun());
    }

    @Test
    @Feature({"ContextualSearch", "TapProperNounSuppression"})
    public void testTapInStartOfTextBufferNotRecognized() {
        setupTapInEnglishStartOfBuffer();
        assertFalse(mEntityHeuristic.isProbablyEnglishProperNoun());
    }

    @Test
    @Feature({"ContextualSearch", "TapProperNounSuppression"})
    public void testTapInSingleWordAfterCommaNotRecognized() {
        setupTapInWordAfterComma();
        assertFalse(mEntityHeuristic.isProbablyEnglishProperNoun());
    }
}
