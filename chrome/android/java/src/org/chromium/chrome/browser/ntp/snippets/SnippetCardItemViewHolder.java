// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.snippets.SnippetsManager.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SnippetsManager.SnippetListItem;

/**
 * A class that represents the view for a single card snippet.
 */
class SnippetCardItemViewHolder extends SnippetListItemViewHolder implements View.OnClickListener {
    public TextView mHeadlineTextView;
    public TextView mPublisherTextView;
    public TextView mArticleSnippetTextView;
    public TextView mReadMoreLinkTextView;
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
    public SnippetCardItemViewHolder(View cardView, SnippetsManager manager) {
        super(cardView, manager);

        cardView.setOnClickListener(this);
        mThumbnailView = (ImageView) cardView.findViewById(R.id.article_thumbnail);
        mHeadlineTextView = (TextView) cardView.findViewById(R.id.article_headline);
        mPublisherTextView = (TextView) cardView.findViewById(R.id.article_publisher);
        mArticleSnippetTextView = (TextView) cardView.findViewById(R.id.article_snippet);
        mReadMoreLinkTextView = (TextView) cardView.findViewById(R.id.read_more_link);
        mReadMoreLinkTextView.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                loadUrl(mUrl);
                RecordUserAction.record("MobileNTP.Snippets.Click");
                RecordHistogram.recordSparseSlowlyHistogram(
                        "NewTabPage.Snippets.CardClicked", mPosition);
                RecordHistogram.recordEnumeratedHistogram(SnippetsManager.SNIPPETS_STATE_HISTOGRAM,
                        SnippetsManager.SNIPPETS_CLICKED, SnippetsManager.NUM_SNIPPETS_ACTIONS);
            }
        });
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
        // Toggle visibility of snippet text
        int visibility =
                (mArticleSnippetTextView.getVisibility() == View.GONE) ? View.VISIBLE : View.GONE;
        mArticleSnippetTextView.setVisibility(visibility);
        mReadMoreLinkTextView.setVisibility(visibility);

        String action = visibility == View.VISIBLE ? "MobileNTP.Snippets.ShowMore"
                                                   : "MobileNTP.Snippets.ShowLess";
        String histogram = visibility == View.VISIBLE ? "NewTabPage.Snippets.CardExpanded"
                                                      : "NewTabPage.Snippets.CardHidden";
        RecordUserAction.record(action);
        RecordHistogram.recordSparseSlowlyHistogram(histogram, mPosition);
    }

    @Override
    public void onBindViewHolder(SnippetListItem snippetItem) {
        SnippetArticle item = (SnippetArticle) snippetItem;

        mHeadlineTextView.setText(item.mTitle);
        mPublisherTextView.setText(item.mPublisher);
        mArticleSnippetTextView.setText(item.mSnippet);
        mUrl = item.mUrl;
        mPosition = item.mPosition;

        item.setThumbnailOnView(mThumbnailView);
    }
}
