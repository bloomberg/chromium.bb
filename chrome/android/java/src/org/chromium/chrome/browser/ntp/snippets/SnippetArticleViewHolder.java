// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.text.format.DateUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.cards.NewTabPageListItem;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;

/**
 * A class that represents the view for a single card snippet.
 */
public class SnippetArticleViewHolder extends NewTabPageViewHolder implements View.OnClickListener {
    private final NewTabPageManager mNewTabPageManager;
    public TextView mHeadlineTextView;
    public TextView mPublisherTextView;
    public TextView mArticleSnippetTextView;
    public ImageView mThumbnailView;
    public String mUrl;
    public int mPosition;

    /**
     * Creates the CardView object to display snippets information
     *
     * @param parent The parent view for the card
     * @return a CardView object for displaying snippets
     */
    public static View createView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.new_tab_page_snippets_card, parent, false);
    }

    /**
     * Constructs a SnippetCardItemView item used to display snippets
     *
     * @param cardView The View for the snippet card
     * @param manager The SnippetsManager object used to open an article
     */
    public SnippetArticleViewHolder(View cardView, NewTabPageManager manager) {
        super(cardView);

        mNewTabPageManager = manager;
        cardView.setOnClickListener(this);
        mThumbnailView = (ImageView) cardView.findViewById(R.id.article_thumbnail);
        mHeadlineTextView = (TextView) cardView.findViewById(R.id.article_headline);
        mPublisherTextView = (TextView) cardView.findViewById(R.id.article_publisher);
        mArticleSnippetTextView = (TextView) cardView.findViewById(R.id.article_snippet);
        cardView.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View v) {
                RecordHistogram.recordSparseSlowlyHistogram(
                        "NewTabPage.Snippets.CardShown", mPosition);
            }

            @Override
            public void onViewDetachedFromWindow(View v) {}
        });
    }

    @Override
    public void onClick(View v) {
        mNewTabPageManager.open(mUrl);
        RecordUserAction.record("MobileNTP.Snippets.Click");
        RecordHistogram.recordSparseSlowlyHistogram("NewTabPage.Snippets.CardClicked", mPosition);
        NewTabPageUma.recordSnippetAction(NewTabPageUma.SNIPPETS_ACTION_CLICKED);
        NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_SNIPPET);
    }

    @Override
    public void onBindViewHolder(NewTabPageListItem article) {
        SnippetArticle item = (SnippetArticle) article;

        mHeadlineTextView.setText(item.mTitle);
        mPublisherTextView.setText(
                DateUtils.getRelativeTimeSpanString(item.mTimestamp, System.currentTimeMillis(),
                        DateUtils.MINUTE_IN_MILLIS, DateUtils.FORMAT_ABBREV_RELATIVE));
        mArticleSnippetTextView.setText(item.mPreviewText);
        mUrl = item.mUrl;
        mPosition = item.mPosition;

        item.setThumbnailOnView(mThumbnailView);
    }

    @Override
    public boolean isDismissable() {
        return true;
    }
}
