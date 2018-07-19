// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.content.Context;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;

/**
 * View for the header of a section of cards.
 */
public class SectionHeaderView extends LinearLayout implements View.OnClickListener {
    private TextView mTitleView;
    private ImageView mIconView;
    private @Nullable SectionHeader mHeader;

    public SectionHeaderView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitleView = findViewById(org.chromium.chrome.R.id.header_title);
        mIconView = findViewById(org.chromium.chrome.R.id.header_icon);
    }

    @Override
    public void onClick(View view) {
        assert mHeader.isExpandable() : "onClick() is called on a non-expandable section header.";
        mHeader.toggleHeader();
        SuggestionsMetrics.recordExpandableHeaderTapped(mHeader.isExpanded());
        SuggestionsMetrics.recordArticlesListVisible();
    }

    /** @param header The {@link SectionHeader} that holds the data for this class. */
    public void setHeader(SectionHeader header) {
        mHeader = header;
        if (mHeader == null) return;

        mTitleView.setText(header.getHeaderText());
        mIconView.setVisibility(header.isExpandable() ? View.VISIBLE : View.GONE);
        updateIconDrawable();
        setOnClickListener(header.isExpandable() ? this : null);
    }

    /** Update the image resource for the icon view based on whether the header is expanded. */
    public void updateIconDrawable() {
        if (mHeader == null || !mHeader.isExpandable()) return;
        mIconView.setImageResource(mHeader.isExpanded() ? R.drawable.ic_expand_less_black_24dp
                                                        : R.drawable.ic_expand_more_black_24dp);
        mIconView.setContentDescription(mIconView.getResources().getString(mHeader.isExpanded()
                        ? R.string.accessibility_collapse_section_header
                        : R.string.accessibility_expand_section_header));
    }
}
