// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.answer.AnswerSuggestionViewProperties.AnswerIcon;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A mechanism binding AnswerSuggestion properties to its view. */
public class AnswerSuggestionViewBinder {
    /** @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object) */
    public static void bind(
            PropertyModel model, AnswerSuggestionView view, PropertyKey propertyKey) {
        if (AnswerSuggestionViewProperties.DELEGATE.equals(propertyKey)) {
            view.setDelegate(model.get(AnswerSuggestionViewProperties.DELEGATE));
        } else if (SuggestionCommonProperties.USE_DARK_COLORS.equals(propertyKey)) {
            view.setUseDarkColors(model.get(SuggestionCommonProperties.USE_DARK_COLORS));
        } else if (AnswerSuggestionViewProperties.ANSWER_IMAGE.equals(propertyKey)) {
            view.setIconBitmap(model.get(AnswerSuggestionViewProperties.ANSWER_IMAGE));
        } else if (AnswerSuggestionViewProperties.ANSWER_ICON_TYPE.equals(propertyKey)) {
            int type = model.get(AnswerSuggestionViewProperties.ANSWER_ICON_TYPE);

            if (type == AnswerIcon.UNDEFINED) return;
            int drawableId = R.drawable.ic_omnibox_page;
            switch (type) {
                case AnswerIcon.CALCULATOR:
                    drawableId = R.drawable.ic_equals_sign_round;
                    break;
                case AnswerIcon.DICTIONARY:
                    drawableId = R.drawable.ic_book_round;
                    break;
                case AnswerIcon.FINANCE:
                    drawableId = R.drawable.ic_swap_vert_round;
                    break;
                case AnswerIcon.KNOWLEDGE:
                    drawableId = R.drawable.ic_google_round;
                    break;
                case AnswerIcon.SUNRISE:
                    drawableId = R.drawable.ic_wb_sunny_round;
                    break;
                case AnswerIcon.TRANSLATION:
                    drawableId = R.drawable.logo_translate_round;
                    break;
                case AnswerIcon.WEATHER:
                    drawableId = R.drawable.logo_partly_cloudy_light;
                    break;
                case AnswerIcon.EVENT:
                    drawableId = R.drawable.ic_event_round;
                    break;
                case AnswerIcon.CURRENCY:
                    drawableId = R.drawable.ic_loop_round;
                    break;
                case AnswerIcon.SPORTS:
                    drawableId = R.drawable.ic_google_round;
                    break;
                default:
                    assert false : "Invalid answer type: " + type;
                    break;
            }

            view.setFallbackIconRes(drawableId);
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT.equals(propertyKey)) {
            view.setLine1TextContent(model.get(AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT.equals(propertyKey)) {
            view.setLine2TextContent(model.get(AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION.equals(
                           propertyKey)) {
            view.setLine1AccessibilityDescription(model.get(
                    AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION));
        } else if (AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION.equals(
                           propertyKey)) {
            view.setLine2AccessibilityDescription(model.get(
                    AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION));
        }
    }
}
