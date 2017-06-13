// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.support.annotation.IdRes;
import android.support.annotation.StringRes;
import android.text.Layout;
import android.text.style.BulletSpan;
import android.text.style.ForegroundColorSpan;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.ui.text.SpanApplier;

/**
 * The Material Design New Tab Page for use in the Incognito profile. This is an extension
 * of the IncognitoNewTabPageView class with improved text content and a more responsive design.
 */
public class IncognitoNewTabPageViewMD extends IncognitoNewTabPageView {
    private final Context mContext;
    private final DisplayMetrics mMetrics;

    private int mWidthDp;
    private int mHeightDp;

    private LinearLayout mContainer;
    private TextView mHeader;
    private TextView[] mParagraphs;

    static class IncognitoBulletSpan extends BulletSpan {
        public IncognitoBulletSpan() {
            super(0 /* gapWidth */);
        }

        @Override
        public void drawLeadingMargin(Canvas c, Paint p, int x, int dir, int top, int baseline,
                int bottom, CharSequence text, int start, int end, boolean first, Layout l) {
            // Do not draw the standard bullet point. We will include the Unicode bullet point
            // symbol in the text instead.
        }
    }

    /** Default constructor needed to inflate via XML. */
    public IncognitoNewTabPageViewMD(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mMetrics = mContext.getResources().getDisplayMetrics();
    }

    private int pxToDp(int px) {
        return (int) Math.ceil(px / mMetrics.density);
    }

    private int dpToPx(int dp) {
        return (int) Math.ceil(dp * mMetrics.density);
    }

    private int spToPx(int sp) {
        return (int) Math.ceil(sp * mMetrics.scaledDensity);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        populateBulletpoints(R.id.new_tab_incognito_features, R.string.new_tab_otr_not_saved);
        populateBulletpoints(R.id.new_tab_incognito_warning, R.string.new_tab_otr_visible);

        mContainer = (LinearLayout) findViewById(R.id.new_tab_incognito_container);
        mHeader = (TextView) findViewById(R.id.new_tab_incognito_title);
        mParagraphs = new TextView[] {
                (TextView) findViewById(R.id.new_tab_incognito_subtitle),
                (TextView) findViewById(R.id.new_tab_incognito_features),
                (TextView) findViewById(R.id.new_tab_incognito_warning),
                (TextView) findViewById(R.id.learn_more),
        };
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int mNewWidthDp = pxToDp(View.MeasureSpec.getSize(widthMeasureSpec));
        int mNewHeightDp = pxToDp(View.MeasureSpec.getSize(heightMeasureSpec));

        boolean needsReflow = mWidthDp != mNewWidthDp || mHeightDp != mNewHeightDp;

        mWidthDp = mNewWidthDp;
        mHeightDp = mNewHeightDp;

        if (needsReflow) reflowLayout();

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * @param element Resource ID of the element to be populated with the bulletpoints.
     * @param content String ID to serve as the text of |element|. Must contain an <em></em> span,
     *         which will be emphasized, and three <li> items, which will be converted to
     *         bulletpoints.
     * Populates |element| with |content|.
     */
    private void populateBulletpoints(@IdRes int element, @StringRes int content) {
        TextView view = (TextView) findViewById(element);
        String text = mContext.getResources().getString(content);

        // TODO(msramek): Unfortunately, our strings are missing the closing "</li>" tag, which
        // is not a problem when they're used in the Desktop WebUI (omitting the tag is valid in
        // HTML5), but it is a problem for SpanApplier. Update the strings and remove this regex.
        // Note that modifying the strings is a non-trivial operation as they went through a special
        // translation process.
        text = text.replaceAll("<li>([^<]+)\n", "<li>$1</li>\n");

        // Format the bulletpoints:
        //   - Disambiguate the <li></li> spans for SpanApplier.
        //   - Add the bulletpoint symbols (Unicode BULLET U+2022)
        //   - Remove leading whitespace (caused by formatting in the .grdp file)
        text = text.replaceFirst(" +<li>([^<]*)</li>", "<li1>\u2022 $1</li1>");
        text = text.replaceFirst(" +<li>([^<]*)</li>", "<li2>\u2022 $1</li2>");
        text = text.replaceFirst(" +<li>([^<]*)</li>", "<li3>\u2022 $1</li3>");

        // Remove the <ul></ul> tags which serve no purpose here, including the whitespace around
        // them.
        text = text.replaceAll(" +</?ul>\\n?", "");

        view.setText(SpanApplier.applySpans(text,
                new SpanApplier.SpanInfo("<em>", "</em>",
                        new ForegroundColorSpan(ApiCompatibilityUtils.getColor(
                                mContext.getResources(), R.color.incognito_emphasis))),
                new SpanApplier.SpanInfo("<li1>", "</li1>", new IncognitoBulletSpan()),
                new SpanApplier.SpanInfo("<li2>", "</li2>", new IncognitoBulletSpan()),
                new SpanApplier.SpanInfo("<li3>", "</li3>", new IncognitoBulletSpan())));
    }

    /** Reflows the layout based on the current window size. */
    private void reflowLayout() {
        adjustTypography();
        adjustLayout();
        adjustIcon();
    }

    /** Adjusts the font settings. */
    private void adjustTypography() {
        if (mWidthDp <= 240 || mHeightDp <= 320) {
            // Small text on small screens.
            mHeader.setTextSize(20 /* sp */);
            mHeader.setLineSpacing(spToPx(4) /* add */, 1 /* mult */); // 20sp + 4sp = 24sp

            for (TextView paragraph : mParagraphs) paragraph.setTextSize(12 /* sp */);
        } else {
            // Large text on large screens.
            mHeader.setTextSize(24 /* sp */);
            mHeader.setLineSpacing(spToPx(8) /* add */, 1 /* mult */); // 24sp + 8sp = 32sp

            for (TextView paragraph : mParagraphs) paragraph.setTextSize(14 /* sp */);
        }

        // Paragraph line spacing is constant +6sp, defined in R.layout.new_tab_page_incognito_md.
    }

    /** Adjusts the paddings and margins. */
    private void adjustLayout() {
        int paddingHorizontalDp;
        int paddingVerticalDp;

        if (mWidthDp <= 720) {
            // Small screens.
            paddingHorizontalDp = mWidthDp <= 240 ? 24 : 32;
            paddingVerticalDp = mHeightDp <= 480 ? 32 : (mHeightDp <= 600 ? 48 : 72);
        } else {
            // Large screens.
            paddingHorizontalDp = 0; // Should not be necessary on a screen this large.
            paddingVerticalDp = mHeightDp <= 320 ? 16 : 72;
        }

        mContainer.setPadding(dpToPx(paddingHorizontalDp), dpToPx(paddingVerticalDp),
                dpToPx(paddingHorizontalDp), dpToPx(paddingVerticalDp));

        int spacingPx =
                (int) Math.ceil(mParagraphs[0].getTextSize() * (mHeightDp <= 600 ? 1 : 1.5));

        for (TextView paragraph : mParagraphs) {
            ((LinearLayout.LayoutParams) paragraph.getLayoutParams())
                    .setMargins(0, spacingPx, 0, 0);
        }

        ((LinearLayout.LayoutParams) mHeader.getLayoutParams()).setMargins(0, spacingPx, 0, 0);
    }

    /** Adjust the Incognito icon. */
    private void adjustIcon() {
        // TODO(msramek): Instead of stretching the icon, we should have differently-sized versions,
        // or, if possible, reuse an icon intended for a higher DPI.
        int sizeDp;
        if (mWidthDp <= 720) {
            sizeDp = (mWidthDp <= 240 || mHeightDp <= 480) ? 48 : 72;
        } else {
            sizeDp = mHeightDp <= 480 ? 72 : 120;
        }

        ImageView icon = (ImageView) findViewById(R.id.new_tab_incognito_icon);
        icon.getLayoutParams().width = dpToPx(sizeDp);
        icon.getLayoutParams().height = dpToPx(sizeDp);
    }
}
