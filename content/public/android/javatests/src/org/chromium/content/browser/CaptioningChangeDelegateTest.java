// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.graphics.Color;
import android.graphics.Typeface;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.content.browser.accessibility.captioning.CaptioningChangeDelegate;
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
     * Typeface.DEFAULT may be equivalent to another Typeface such as Typeface.SANS_SERIF
     * so this test ensures that each typeface returns DEFAULT_CAPTIONING_PREF_VALUE if it is
     * equal to Typeface.DEFAULT or returns an explicit font family otherwise.
     */
    @SmallTest
    public void testClosedCaptionFont() {
        ClosedCaptionFont font = ClosedCaptionFont.fromSystemFont(null);
        assertEquals(DEFAULT_CAPTIONING_PREF_VALUE, font.getFontFamily());

        font = ClosedCaptionFont.fromSystemFont(Typeface.DEFAULT);
        assertEquals(DEFAULT_CAPTIONING_PREF_VALUE, font.getFontFamily());

        font = ClosedCaptionFont.fromSystemFont(Typeface.DEFAULT_BOLD);
        assertEquals(DEFAULT_CAPTIONING_PREF_VALUE, font.getFontFamily());

        font = ClosedCaptionFont.fromSystemFont(Typeface.MONOSPACE);
        if (Typeface.MONOSPACE.equals(Typeface.DEFAULT)) {
            assertEquals(DEFAULT_CAPTIONING_PREF_VALUE, font.getFontFamily());
        } else {
            assertEquals("monospace", font.getFontFamily());
        }

        font = ClosedCaptionFont.fromSystemFont(Typeface.SANS_SERIF);
        if (Typeface.SANS_SERIF.equals(Typeface.DEFAULT)) {
            assertEquals(DEFAULT_CAPTIONING_PREF_VALUE, font.getFontFamily());
        } else {
            assertEquals("sans-serif", font.getFontFamily());
        }

        font = ClosedCaptionFont.fromSystemFont(Typeface.SERIF);
        if (Typeface.SERIF.equals(Typeface.DEFAULT)) {
            assertEquals(DEFAULT_CAPTIONING_PREF_VALUE, font.getFontFamily());
        } else {
            assertEquals("serif", font.getFontFamily());
        }
    }
}
