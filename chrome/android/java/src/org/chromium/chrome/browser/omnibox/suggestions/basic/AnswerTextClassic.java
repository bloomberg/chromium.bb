// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.content.Context;
import android.graphics.Paint;
import android.support.annotation.StyleRes;
import android.text.TextPaint;
import android.text.style.MetricAffectingSpan;
import android.text.style.TextAppearanceSpan;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.components.omnibox.AnswerTextType;
import org.chromium.components.omnibox.SuggestionAnswer;

/**
 * AnswerTextNewLayout builds Omnibox styled Answer suggestion texts for classic answer layouts.
 */
class AnswerTextClassic extends AnswerText {
    private static final String TAG = "AnswerTextClassic";
    private final float mDensity;

    /**
     * Convert SuggestionAnswer to array of elements that directly translate to user-presented
     * content.
     *
     * @param context Current context.
     * @param answer Specifies answer to be converted.
     * @param density Screen density which will be used to properly size and layout images and top-
     *                aligned text.
     * @return array of AnswerText elements to use to construct suggestion item.
     */
    static AnswerText[] from(Context context, SuggestionAnswer answer, final float density) {
        AnswerText[] result = new AnswerText[2];

        result[0] = new AnswerTextClassic(context, answer.getFirstLine(), density);
        result[1] = new AnswerTextClassic(context, answer.getSecondLine(), density);

        // Trim number of presented query lines.
        result[0].mMaxLines = 1;

        return result;
    }

    /**
     * Create new instance of AnswerTextClassic.
     *
     * @param context Current context.
     * @param line Suggestion line that will be converted to Answer Text.
     * @param density Screen density.
     */
    AnswerTextClassic(Context context, SuggestionAnswer.ImageLine line, float density) {
        super(context);
        mDensity = density;
        build(line);
    }

    /**
     * Return the TextAppearanceSpan array specifying text decorations for a given field type.
     *
     * @param type The answer type as specified at http://goto.google.com/ais_api.
     * @return TextAppearanceSpan array specifying styles to be used to present text field.
     */
    @Override
    protected MetricAffectingSpan[] getAppearanceForText(@AnswerTextType int type) {
        @StyleRes
        int res = 0;
        switch (type) {
            case AnswerTextType.TOP_ALIGNED:
                TextAppearanceSpan style = new TextAppearanceSpan(
                        mContext, R.style.TextAppearance_OmniboxAnswerTextSmall);
                return new MetricAffectingSpan[] {
                        style, new TopAlignedSpan(style.getTextSize(), mHeightSp, mDensity)};

            case AnswerTextType.DESCRIPTION_NEGATIVE:
                res = R.style.TextAppearance_OmniboxAnswerDescriptionNegative;
                break;

            case AnswerTextType.DESCRIPTION_POSITIVE:
                res = R.style.TextAppearance_OmniboxAnswerDescriptionPositive;
                break;

            case AnswerTextType.SUGGESTION:
                res = R.style.TextAppearance_BlackTitle1;
                break;

            case AnswerTextType.PERSONALIZED_SUGGESTION:
                res = R.style.TextAppearance_BlackTitle1;
                break;

            case AnswerTextType.ANSWER_TEXT_MEDIUM:
                res = R.style.TextAppearance_OmniboxAnswerTextMedium;
                break;

            case AnswerTextType.ANSWER_TEXT_LARGE:
                res = R.style.TextAppearance_OmniboxAnswerTextLarge;
                break;

            case AnswerTextType.SUGGESTION_SECONDARY_TEXT_SMALL:
                res = R.style.TextAppearance_BlackCaption;
                break;

            case AnswerTextType.SUGGESTION_SECONDARY_TEXT_MEDIUM:
                res = R.style.TextAppearance_BlackHint2;
                break;

            default:
                Log.w(TAG, "Unknown answer type: " + type);
                res = R.style.TextAppearance_BlackHint2;
                break;
        }

        return new TextAppearanceSpan[] {new TextAppearanceSpan(mContext, res)};
    }

    /**
     * Aligns the top of the spanned text with the top of some other specified text height. This is
     * done by calculating the ascent of both text heights and shifting the baseline of the spanned
     * text by the difference.  As a result, "top aligned" means the top of the ascents are
     * aligned, which looks as expected in most cases (some glyphs in some fonts are drawn above
     * the top of the ascent).
     */
    private static class TopAlignedSpan extends MetricAffectingSpan {
        private final int mTextHeightSp;
        private final int mMaxTextHeightSp;
        private final float mDensity;

        private Integer mBaselineShift;

        /**
         * Constructor for TopAlignedSpan.
         *
         * @param textHeightSp The total height in SP of the text covered by this span.
         * @param maxTextHeightSp The total height in SP of the text we wish to top-align with.
         * @param density The display density.
         */
        public TopAlignedSpan(int textHeightSp, int maxTextHeightSp, float density) {
            mTextHeightSp = textHeightSp;
            mMaxTextHeightSp = maxTextHeightSp;
            mDensity = density;
        }

        @Override
        public void updateDrawState(TextPaint tp) {
            initBaselineShift(tp);
            tp.baselineShift += mBaselineShift;
        }

        @Override
        public void updateMeasureState(TextPaint tp) {
            initBaselineShift(tp);
            tp.baselineShift += mBaselineShift;
        }

        private void initBaselineShift(TextPaint tp) {
            if (mBaselineShift != null) return;
            Paint.FontMetrics metrics = tp.getFontMetrics();
            float ascentProportion = metrics.ascent / (metrics.top - metrics.bottom);

            int textAscentPx = (int) (mTextHeightSp * ascentProportion * mDensity);
            int maxTextAscentPx = (int) (mMaxTextHeightSp * ascentProportion * mDensity);

            mBaselineShift = -(maxTextAscentPx - textAscentPx); // Up is -y.
        }
    }
}
