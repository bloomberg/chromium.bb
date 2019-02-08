// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.content.Context;
import android.text.Html;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.MetricAffectingSpan;
import android.text.style.TextAppearanceSpan;

import org.chromium.components.omnibox.AnswerTextType;
import org.chromium.components.omnibox.SuggestionAnswer;

import java.util.List;

/**
 * AnswerText specifies details to be presented in a single line of omnibox suggestion.
 */
abstract class AnswerText {
    final Context mContext;
    /** Density of current display. */
    final float mDensity;
    /** Content of the line of text in omnibox suggestion. */
    SpannableStringBuilder mText;
    /**
     * Height of the mText.
     * Each AnswerText can be a combination of multiple text styles (both sizes and colors).
     * This height holds either
     * - running maximum (during build phase)
     * - final maximum (after build phase)
     * height of the text contained in mText.
     * Note: since all our AnswerTexts always use the largest text as the very first span of the
     * whole content, it is safe to assume that this field contains the height of the largest text
     * element in all cases, except when computing styles for the first span (= during the very
     * first call to getAppearanceForText()).
     */
    int mHeightSp;
    /** Whether content can wrap around to present more details. */
    int mMaxLines;

    /**
     * Create new instance of AnswerText.
     *
     * @param context Current context.
     */
    AnswerText(Context context) {
        mContext = context;
        mDensity = context.getResources().getDisplayMetrics().density;
    }

    /**
     * Builds a Spannable containing all of the styled text in the supplied ImageLine.
     *
     * @param line All text fields within this line will be used to build the resulting content.
     * @param delegate Callback converting AnswerTextType to an array of TextAppearanceSpan objects.
     */
    protected void build(SuggestionAnswer.ImageLine line) {
        mText = new SpannableStringBuilder();
        mMaxLines = 1;

        // This method also computes height of the entire text span.
        // Ensure we're not rebuilding or appending once AnswerText has been constructed.
        assert mHeightSp == 0;

        List<SuggestionAnswer.TextField> textFields = line.getTextFields();
        for (int i = 0; i < textFields.size(); i++) {
            appendAndStyleText(textFields.get(i));
            if (textFields.get(i).hasNumLines()) {
                mMaxLines = Math.max(mMaxLines, Math.min(3, textFields.get(i).getNumLines()));
            }
        }

        if (line.hasAdditionalText()) {
            mText.append("  ");
            appendAndStyleText(line.getAdditionalText());
        }
        if (line.hasStatusText()) {
            mText.append("  ");
            appendAndStyleText(line.getStatusText());
        }
    }

    /**
     * Append the styled text in textField to the supplied builder.
     *
     * @param textField The text field (with text and type) to append.
     */
    @SuppressWarnings("deprecation") // Update usage of Html.fromHtml when API min is 24
    private void appendAndStyleText(SuggestionAnswer.TextField textField) {
        MetricAffectingSpan[] styles = getAppearanceForText(textField.getType());
        // Determine the maximum height of the TextAppearanceSpans that are applied for this field.
        for (MetricAffectingSpan style : styles) {
            if (!(style instanceof TextAppearanceSpan)) continue;
            TextAppearanceSpan textStyle = (TextAppearanceSpan) style;
            int textHeightSp = (int) (textStyle.getTextSize() / mDensity);
            if (mHeightSp < textHeightSp) mHeightSp = textHeightSp;
        }

        // Unescape HTML entities (e.g. "&quot;", "&gt;").
        String text = Html.fromHtml(textField.getText()).toString();

        // Append as HTML (answer responses contain simple markup).
        int start = mText.length();
        mText.append(Html.fromHtml(text));
        int end = mText.length();

        for (MetricAffectingSpan style : styles) {
            mText.setSpan(style, start, end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
    }

    /**
     * Return the TextAppearanceSpan array specifying text decorations for a given field type.
     *
     * @param type The answer type as specified at http://goto.google.com/ais_api.
     * @return TextAppearanceSpan array specifying styles to be used to present text field.
     */
    protected abstract MetricAffectingSpan[] getAppearanceForText(@AnswerTextType int type);
}
