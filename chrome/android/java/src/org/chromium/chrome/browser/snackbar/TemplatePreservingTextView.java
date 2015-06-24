// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar;

import android.content.Context;
import android.support.v7.widget.AppCompatTextView;
import android.text.TextPaint;
import android.text.TextUtils;
import android.text.TextUtils.TruncateAt;
import android.util.AttributeSet;
import android.widget.TextView;

/**
 * A {@link AppCompatTextView} that properly clips content within a template, instead of clipping
 * the templated content.
 */
public class TemplatePreservingTextView extends AppCompatTextView {
    private static final String DEFAULT_TEMPLATE = "%1$s";
    private String mTemplate;
    private CharSequence mContent;

    /**
     * Builds an instance of an {@link TemplatePreservingTextView}.
     * @param context A {@link Context} instance to build this {@link TextView} in.
     * @param attrs   An {@link AttributeSet} instance.
     */
    public TemplatePreservingTextView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Update template for undo text
     * @param template Template format string (eg. "Close %s")
     */
    public void setTemplate(String template) {
        mTemplate = TextUtils.isEmpty(template) ? DEFAULT_TEMPLATE : template;
    }

    /**
     * This will take {@code text} and apply it to the internal template, building a new
     * {@link String} to set.  This {code text} will be automatically truncated to fit within
     * the template as best as possible, making sure the template does not get clipped.
     */
    @Override
    public void setText(CharSequence text, BufferType type) {
        final int availWidth = getWidth() - getPaddingLeft() - getPaddingRight();
        setClippedText(text, availWidth);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final int availWidth =
                MeasureSpec.getSize(widthMeasureSpec) - getPaddingLeft() - getPaddingRight();
        setClippedText(mContent, availWidth);

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    private void setClippedText(CharSequence text, int availWidth) {
        mContent = text != null ? text : "";

        if (mTemplate == null) {
            super.setText(mContent, BufferType.SPANNABLE);
            return;
        }

        final TextPaint paint = getPaint();

        // Calculate the width the template takes.
        final String emptyTemplate = String.format(mTemplate, "");
        final float emptyTemplateWidth = paint.measureText(emptyTemplate);

        // Calculate the available width for the content.
        final float contentWidth = Math.max(availWidth - emptyTemplateWidth, 0.f);

        // Ellipsize the content to the available width.
        CharSequence clipped = TextUtils.ellipsize(mContent, paint, contentWidth, TruncateAt.END);

        // Build the full string, which should fit within availWidth.
        String finalContent = String.format(mTemplate, clipped);

        // BufferType.SPANNABLE is required so that TextView.getIterableTextForAccessibility()
        // doesn't call our custom setText(). See crbug.com/449311
        super.setText(finalContent, BufferType.SPANNABLE);

        // Set the content description to the non-ellipsized text
        setContentDescription(String.format(mTemplate, mContent));
    }
}