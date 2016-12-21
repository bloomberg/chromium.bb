// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility.captioning;

import android.graphics.Color;
import android.graphics.Typeface;
import android.support.test.filters.SmallTest;

import org.chromium.content.browser.accessibility.captioning.CaptioningChangeDelegate.ClosedCaptionEdgeAttribute;
import org.chromium.content.browser.accessibility.captioning.CaptioningChangeDelegate.ClosedCaptionFont;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
  * Test suite to ensure that platform settings are translated to CSS appropriately
  */
public class CaptioningChangeDelegateTest extends ContentShellTestBase {
    private static final String DEFAULT_CAPTIONING_PREF_VALUE =
            CaptioningChangeDelegate.DEFAULT_CAPTIONING_PREF_VALUE;

    @SmallTest
    public void testFontScaleToPercentage() {
        String result = CaptioningChangeDelegate.androidFontScaleToPercentage(0f);
        assertEquals("0%", result);

        result = CaptioningChangeDelegate.androidFontScaleToPercentage(0.000f);
        assertEquals("0%", result);

        result = CaptioningChangeDelegate.androidFontScaleToPercentage(0.25f);
        assertEquals("25%", result);

        result = CaptioningChangeDelegate.androidFontScaleToPercentage(1f);
        assertEquals("100%", result);

        result = CaptioningChangeDelegate.androidFontScaleToPercentage(1.5f);
        assertEquals("150%", result);

        result = CaptioningChangeDelegate.androidFontScaleToPercentage(0.50125f);
        assertEquals("50%", result);

        result = CaptioningChangeDelegate.androidFontScaleToPercentage(0.50925f);
        assertEquals("51%", result);
    }

    @SmallTest
    public void testAndroidColorToCssColor() {
        String result = CaptioningChangeDelegate.androidColorToCssColor(null);
        assertEquals(DEFAULT_CAPTIONING_PREF_VALUE, result);

        result = CaptioningChangeDelegate.androidColorToCssColor(Color.BLACK);
        assertEquals("rgba(0, 0, 0, 1)", result);

        result = CaptioningChangeDelegate.androidColorToCssColor(Color.WHITE);
        assertEquals("rgba(255, 255, 255, 1)", result);

        result = CaptioningChangeDelegate.androidColorToCssColor(Color.BLUE);
        assertEquals("rgba(0, 0, 255, 1)", result);

        // Transparent-black
        result = CaptioningChangeDelegate.androidColorToCssColor(0x00000000);
        assertEquals("rgba(0, 0, 0, 0)", result);

        // Transparent-white
        result = CaptioningChangeDelegate.androidColorToCssColor(0x00FFFFFF);
        assertEquals("rgba(255, 255, 255, 0)", result);

        // 50% opaque blue
        result = CaptioningChangeDelegate.androidColorToCssColor(0x7f0000ff);
        assertEquals("rgba(0, 0, 255, 0.5)", result);

        // No alpha information
        result = CaptioningChangeDelegate.androidColorToCssColor(0xFFFFFF);
        assertEquals("rgba(255, 255, 255, 0)", result);
    }

    @SmallTest
    public void testClosedCaptionEdgeAttributeWithDefaults() {
        ClosedCaptionEdgeAttribute edge = ClosedCaptionEdgeAttribute.fromSystemEdgeAttribute(
                null, null);
        assertEquals(DEFAULT_CAPTIONING_PREF_VALUE, edge.getTextShadow());

        edge = ClosedCaptionEdgeAttribute.fromSystemEdgeAttribute(null, "red");
        assertEquals(DEFAULT_CAPTIONING_PREF_VALUE, edge.getTextShadow());

        edge = ClosedCaptionEdgeAttribute.fromSystemEdgeAttribute(0, "red");
        assertEquals(DEFAULT_CAPTIONING_PREF_VALUE, edge.getTextShadow());

        edge = ClosedCaptionEdgeAttribute.fromSystemEdgeAttribute(2, null);
        assertEquals("silver 0.05em 0.05em 0.1em", edge.getTextShadow());

        edge = ClosedCaptionEdgeAttribute.fromSystemEdgeAttribute(2, "");
        assertEquals("silver 0.05em 0.05em 0.1em", edge.getTextShadow());

        edge = ClosedCaptionEdgeAttribute.fromSystemEdgeAttribute(2, "red");
        assertEquals("red 0.05em 0.05em 0.1em", edge.getTextShadow());
    }

    @SmallTest
    public void testClosedCaptionEdgeAttributeWithCustomDefaults() {
        ClosedCaptionEdgeAttribute.setShadowOffset("0.00em");
        ClosedCaptionEdgeAttribute.setDefaultEdgeColor("red");
        ClosedCaptionEdgeAttribute edge = ClosedCaptionEdgeAttribute.fromSystemEdgeAttribute(
                null, null);
        assertEquals(DEFAULT_CAPTIONING_PREF_VALUE, edge.getTextShadow());

        edge = ClosedCaptionEdgeAttribute.fromSystemEdgeAttribute(null, "red");
        assertEquals(DEFAULT_CAPTIONING_PREF_VALUE, edge.getTextShadow());

        edge = ClosedCaptionEdgeAttribute.fromSystemEdgeAttribute(0, "red");
        assertEquals(DEFAULT_CAPTIONING_PREF_VALUE, edge.getTextShadow());

        edge = ClosedCaptionEdgeAttribute.fromSystemEdgeAttribute(2, null);
        assertEquals("red 0.00em 0.00em 0.1em", edge.getTextShadow());

        edge = ClosedCaptionEdgeAttribute.fromSystemEdgeAttribute(2, "silver");
        assertEquals("silver 0.00em 0.00em 0.1em", edge.getTextShadow());
    }

    /**
     * Verifies that certain system fonts always correspond to the default captioning font.
     */
    @SmallTest
    public void testClosedCaptionDefaultFonts() {
        final ClosedCaptionFont nullFont = ClosedCaptionFont.fromSystemFont(null);
        assertEquals(
                "Null typeface should return the default font family.",
                DEFAULT_CAPTIONING_PREF_VALUE, nullFont.getFontFamily());

        final ClosedCaptionFont defaultFont = ClosedCaptionFont.fromSystemFont(Typeface.DEFAULT);
        assertEquals(
                "Typeface.DEFAULT should return the default font family.",
                DEFAULT_CAPTIONING_PREF_VALUE, defaultFont.getFontFamily());

        final ClosedCaptionFont defaultBoldFont = ClosedCaptionFont.fromSystemFont(
                Typeface.DEFAULT_BOLD);
        assertEquals(
                "Typeface.BOLD should return the default font family.",
                DEFAULT_CAPTIONING_PREF_VALUE, defaultBoldFont.getFontFamily());
    }

    /**
     * Typeface.DEFAULT may be equivalent to another Typeface such as Typeface.SANS_SERIF
     * so this test ensures that each typeface returns DEFAULT_CAPTIONING_PREF_VALUE if it is
     * equal to Typeface.DEFAULT or returns an explicit font family otherwise.
     */
    @SmallTest
    public void testClosedCaptionNonDefaultFonts() {
        final ClosedCaptionFont monospaceFont = ClosedCaptionFont.fromSystemFont(
                Typeface.MONOSPACE);
        if (Typeface.MONOSPACE.equals(Typeface.DEFAULT)) {
            assertEquals(
                    "Since the default font is monospace, the default family should be returned.",
                    DEFAULT_CAPTIONING_PREF_VALUE, monospaceFont.getFontFamily());
        } else {
            assertTrue(
                    "Typeface.MONOSPACE should return a monospace font family.",
                    monospaceFont.mFlags.contains(ClosedCaptionFont.Flags.MONOSPACE));
        }

        final ClosedCaptionFont sansSerifFont = ClosedCaptionFont.fromSystemFont(
                Typeface.SANS_SERIF);
        if (Typeface.SANS_SERIF.equals(Typeface.DEFAULT)) {
            assertEquals(
                    "Since the default font is sans-serif, the default family should be returned.",
                    DEFAULT_CAPTIONING_PREF_VALUE, sansSerifFont.getFontFamily());
        } else {
            assertTrue(
                    "Typeface.SANS_SERIF should return a sans-serif font family.",
                    sansSerifFont.mFlags.contains(ClosedCaptionFont.Flags.SANS_SERIF));
        }

        final ClosedCaptionFont serifFont = ClosedCaptionFont.fromSystemFont(Typeface.SERIF);
        if (Typeface.SERIF.equals(Typeface.DEFAULT)) {
            assertEquals(
                    "Since the default font is serif, the default font family should be returned.",
                    DEFAULT_CAPTIONING_PREF_VALUE, serifFont.getFontFamily());
        } else {
            assertTrue(
                    "Typeface.SERIF should return a serif font family.",
                    serifFont.mFlags.contains(ClosedCaptionFont.Flags.SERIF));
        }
    }
}
