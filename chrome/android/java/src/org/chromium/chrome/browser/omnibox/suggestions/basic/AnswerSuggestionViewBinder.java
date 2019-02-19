// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.text.Spannable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties.SuggestionIcon;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties.SuggestionTextContainer;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A mechanism binding AnswerSuggestion properties to its view. */
public class AnswerSuggestionViewBinder {
    /** @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object) */
    public static void bind(
            PropertyModel model, AnswerSuggestionView view, PropertyKey propertyKey) {
        if (SuggestionViewProperties.DELEGATE.equals(propertyKey)) {
            view.setDelegate(model.get(SuggestionViewProperties.DELEGATE));
        } else if (SuggestionCommonProperties.USE_DARK_COLORS.equals(propertyKey)) {
            view.setUseDarkColors(model.get(SuggestionCommonProperties.USE_DARK_COLORS));
        } else if (SuggestionViewProperties.ANSWER_IMAGE.equals(propertyKey)) {
            view.setIconBitmap(model.get(SuggestionViewProperties.ANSWER_IMAGE));
        } else if (SuggestionViewProperties.SUGGESTION_ICON_TYPE.equals(propertyKey)) {
            int type = model.get(SuggestionViewProperties.SUGGESTION_ICON_TYPE);

            if (type == SuggestionIcon.UNDEFINED) return;
            int drawableId = R.drawable.ic_omnibox_page;
            switch (type) {
                case SuggestionIcon.DICTIONARY:
                    drawableId = R.drawable.ic_book_round;
                    break;
                case SuggestionIcon.FINANCE:
                    drawableId = R.drawable.ic_swap_vert_round;
                    break;
                case SuggestionIcon.KNOWLEDGE:
                    drawableId = R.drawable.ic_google_round;
                    break;
                case SuggestionIcon.SUNRISE:
                    drawableId = R.drawable.ic_wb_sunny_round;
                    break;
                case SuggestionIcon.TRANSLATION:
                    drawableId = R.drawable.logo_translate_round;
                    break;
                case SuggestionIcon.WEATHER:
                    drawableId = R.drawable.logo_partly_cloudy_light;
                    break;
                case SuggestionIcon.EVENT:
                    drawableId = R.drawable.ic_event_round;
                    break;
                case SuggestionIcon.CURRENCY:
                    drawableId = R.drawable.ic_loop_round;
                    break;
                case SuggestionIcon.SPORTS:
                    drawableId = R.drawable.ic_google_round;
                    break;
                default:
                    assert false : "Invalid suggestion type: " + type;
                    break;
            }

            view.setFallbackIconRes(drawableId);
        } else if (SuggestionViewProperties.TEXT_LINE_1_TEXT.equals(propertyKey)) {
            SuggestionTextContainer container =
                    model.get(SuggestionViewProperties.TEXT_LINE_1_TEXT);
            Spannable span = null;
            if (container != null) span = container.text;
            view.setLine1TextContent(span);
        } else if (SuggestionViewProperties.TEXT_LINE_2_TEXT.equals(propertyKey)) {
            SuggestionTextContainer container =
                    model.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
            Spannable span = null;
            if (container != null) span = container.text;
            view.setLine2TextContent(span);
        }
    }
}
