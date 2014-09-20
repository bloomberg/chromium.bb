// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.test.ActivityInstrumentationTestCase2;
import android.text.SpannableString;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.content.browser.test.util.TestTouchUtils;


/**
 * Utility functions for dealing with Translate InfoBars.
 */
public class TranslateUtil {
    /**
     * Finds the first clickable span inside a TextView and clicks it.
     *
     * @return True if the panel is opened.
     */
    public static boolean openLanguagePanel(ActivityInstrumentationTestCase2<?> test,
            InfoBar infoBar) {
        View view = infoBar.getContentWrapper().findViewById(R.id.infobar_message);
        if (view == null) {
            return false;
        }

        TextView text = (TextView) view.findViewById(R.id.infobar_message);

        SpannableString spannable = (SpannableString) text.getText();
        ClickableSpan[] clickable =
            spannable.getSpans(0, spannable.length() - 1, ClickableSpan.class);
        if (clickable.length <= 0) {
            return false;
        }

        // Find the approximate coordinates of the first link of the first line of text so we can
        // click there. Clicking on any character of the link will work so instead of focusing on
        // the beginning of the link we add one more character so that finding a valid coordinate
        // is more reliable.
        int x = spannable.getSpanStart(clickable[0]) + 1;
        float nChars = text.getLayout().getLineVisibleEnd(0);

        // Not all characters have the same width but this is a good approximation.
        float sizePerChar =  text.getLayout().getLineWidth(0) / nChars;
        float xPos = text.getPaddingLeft() + (sizePerChar * x);
        float yPos = text.getHeight() / (float) 2;

        TestTouchUtils.singleClickView(test.getInstrumentation(), text, (int) xPos, (int) yPos);

        return verifyInfoBarText(infoBar,
            test.getActivity().getString(R.string.translate_infobar_change_languages));
    }

    public static boolean verifyInfoBarText(InfoBar infoBar, String text) {
        View view = infoBar.getContentWrapper().findViewById(R.id.infobar_message);
        if (view == null) {
            return false;
        }
        String infoBarText = findInfoBarText(view);
        return text.equals(infoBarText);
    }

    private static String findInfoBarText(View view) {
        TextView text = (TextView) view.findViewById(R.id.infobar_message);
        return text != null ? text.getText().toString() : null;
    }
}
