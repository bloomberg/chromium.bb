// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.content.Context;
import android.support.annotation.StyleRes;
import android.text.style.MetricAffectingSpan;
import android.text.style.TextAppearanceSpan;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.components.omnibox.AnswerTextType;
import org.chromium.components.omnibox.AnswerType;
import org.chromium.components.omnibox.SuggestionAnswer;

/**
 * AnswerTextNewLayout builds Omnibox styled Answer suggestion texts for revamped answer layouts.
 */
class AnswerTextNewLayout extends AnswerText {
    private static final String TAG = "AnswerTextNewLayout";
    private final boolean mIsAnswer;

    /**
     * Convert SuggestionAnswer to array of elements that directly translate to user-presented
     * content.
     *
     * @param context Current context.
     * @param answer Specifies answer to be converted.
     * @return array of AnswerText elements to use to construct suggestion item.
     */
    static AnswerText[] from(Context context, SuggestionAnswer answer) {
        AnswerText[] result = new AnswerText[2];

        if (answer.getType() == AnswerType.DICTIONARY) {
            result[0] = new AnswerTextNewLayout(context, answer.getFirstLine(), true);
            result[1] = new AnswerTextNewLayout(context, answer.getSecondLine(), false);
            result[0].mMaxLines = 1;

        } else {
            result[0] = new AnswerTextNewLayout(context, answer.getSecondLine(), true);
            result[1] = new AnswerTextNewLayout(context, answer.getFirstLine(), false);
            result[1].mMaxLines = 1;
        }

        return result;
    }

    /**
     * Create new instance of AnswerTextNewLayout.
     *
     * @param context Current context.
     * @param line Suggestion line that will be converted to Answer Text.
     * @param isAnswerLine True, whether this instance holds answer.
     */
    AnswerTextNewLayout(Context context, SuggestionAnswer.ImageLine line, boolean isAnswerLine) {
        super(context);
        mIsAnswer = isAnswerLine;
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
        return mIsAnswer ? getAppearanceForAnswerText(type) : getAppearanceForQueryText(type);
    }

    /**
     * Return text styles for elements in main line holding answer.
     *
     * @param type The answer type as specified at http://goto.google.com/ais_api.
     * @return array of TextAppearanceSpan objects defining style for the text.
     */
    private MetricAffectingSpan[] getAppearanceForAnswerText(@AnswerTextType int type) {
        @StyleRes
        int res = 0;

        switch (type) {
            case AnswerTextType.DESCRIPTION_NEGATIVE:
                res = R.style.TextAppearance_OmniboxAnswerDescriptionNegativeSmall;
                break;

            case AnswerTextType.DESCRIPTION_POSITIVE:
                res = R.style.TextAppearance_OmniboxAnswerDescriptionPositiveSmall;
                break;

            case AnswerTextType.SUGGESTION_SECONDARY_TEXT_MEDIUM:
                res = R.style.TextAppearance_BlackDisabledText2;
                break;

            case AnswerTextType.SUGGESTION:
            case AnswerTextType.PERSONALIZED_SUGGESTION:
            case AnswerTextType.ANSWER_TEXT_MEDIUM:
            case AnswerTextType.ANSWER_TEXT_LARGE:
            case AnswerTextType.TOP_ALIGNED:
            case AnswerTextType.SUGGESTION_SECONDARY_TEXT_SMALL:
                res = R.style.TextAppearance_BlackTitle1;
                break;

            default:
                Log.w(TAG, "Unknown answer type: " + type);
                res = R.style.TextAppearance_BlackTitle1;
                break;
        }

        return new TextAppearanceSpan[] {new TextAppearanceSpan(mContext, res)};
    }

    /**
     * Return text styles for elements in second line holding query.
     *
     * @param type The answer type as specified at http://goto.google.com/ais_api.
     * @return array of TextAppearanceSpan objects defining style for the text.
     */
    private MetricAffectingSpan[] getAppearanceForQueryText(@AnswerTextType int type) {
        return new TextAppearanceSpan[] {
                new TextAppearanceSpan(mContext, R.style.TextAppearance_BlackHint2)};
    }
}
