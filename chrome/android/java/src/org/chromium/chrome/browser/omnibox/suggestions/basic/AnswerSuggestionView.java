// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.content.Context;
import android.graphics.Bitmap;
import android.support.annotation.DrawableRes;
import android.text.Spannable;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.chrome.R;

/**
 * Container view for omnibox answer suggestions.
 */
public class AnswerSuggestionView extends RelativeLayout {
    private SuggestionViewDelegate mSuggestionDelegate;
    private TextView mTextView1;
    private TextView mTextView2;
    private ImageView mAnswerIcon;

    /** Creates new instance of AnswerSuggestionView. */
    public AnswerSuggestionView(Context context, AttributeSet attributes) {
        super(context, attributes);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTextView1 = findViewById(R.id.omnibox_answer_line_1);
        mTextView2 = findViewById(R.id.omnibox_answer_line_2);
        mAnswerIcon = findViewById(R.id.omnibox_answer_icon);
    }

    /** Specify delegate receiving click/refine events. */
    void setDelegate(SuggestionViewDelegate delegate) {
        mSuggestionDelegate = delegate;
    }

    /**
     * Toggles theme.
     * @param useDarkColors specifies whether UI should use dark theme.
     */
    void setUseDarkColors(boolean useDarkColors) {}

    /**
     * Specifies text content of the first text line.
     * @param text Text to be displayed.
     */
    void setLine1TextContent(Spannable text) {
        mTextView1.setText(text);
    }

    /**
     * Specifies text content of the second text line.
     * @param text Text to be displayed.
     */
    void setLine2TextContent(Spannable text) {
        mTextView2.setText(text);
    }

    /**
     * Specifies image bitmap to be displayed as an answer icon.
     * @param bitmap Decoded image to be displayed as an answer icon.
     */
    void setIconBitmap(Bitmap bitmap) {}

    /**
     * Specifies fallback image to be presented if no image is available.
     * @param res Drawable resource ID to be used in place of answer image.
     */
    void setFallbackIconRes(@DrawableRes int res) {
        mAnswerIcon.setImageResource(res);
    }
}
