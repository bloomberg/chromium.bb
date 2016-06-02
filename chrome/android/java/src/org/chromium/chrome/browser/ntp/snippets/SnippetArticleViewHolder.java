// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.media.ThumbnailUtils;
import android.support.annotation.IntDef;
import android.support.v4.text.BidiFormatter;
import android.text.format.DateUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.cards.NewTabPageListItem;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder;

import java.net.URI;
import java.net.URISyntaxException;

/**
 * A class that represents the view for a single card snippet.
 */
public class SnippetArticleViewHolder extends NewTabPageViewHolder implements View.OnClickListener {
    private static final String TAG = "NtpSnippets";
    private static final String PUBLISHER_FORMAT_STRING = "%s - %s";
    private static final int FADE_IN_ANIMATION_TIME_MS = 300;

    private final NewTabPageManager mNewTabPageManager;
    private final TextView mHeadlineTextView;
    private final TextView mPublisherTextView;
    private final TextView mArticleSnippetTextView;
    private final ImageView mThumbnailView;

    private FetchImageCallback mImageCallback;
    private SnippetArticle mArticle;
    private ViewTreeObserver.OnPreDrawListener mPreDrawObserver;

    @IntDef({CACHE_FOR_URL, CACHE_FOR_DOMAIN, DEFAULT_FAVICON})
    public @interface FaviconSource {}
    public static final int CACHE_FOR_URL = 0;
    public static final int CACHE_FOR_DOMAIN = 1;
    public static final int DEFAULT_FAVICON = 2;

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
    public SnippetArticleViewHolder(final View cardView, NewTabPageManager manager) {
        super(cardView);

        mNewTabPageManager = manager;
        cardView.setOnClickListener(this);
        mThumbnailView = (ImageView) cardView.findViewById(R.id.article_thumbnail);
        mHeadlineTextView = (TextView) cardView.findViewById(R.id.article_headline);
        mPublisherTextView = (TextView) cardView.findViewById(R.id.article_publisher);
        mArticleSnippetTextView = (TextView) cardView.findViewById(R.id.article_snippet);

        mPreDrawObserver = new ViewTreeObserver.OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                if (mArticle != null && !mArticle.impressionTracked()) {
                    Rect r = new Rect(0, 0, cardView.getWidth(), cardView.getHeight());
                    cardView.getParent().getChildVisibleRect(cardView, r, null);
                    // Track impression if at least one third of the snippet is shown.
                    if (r.height() >= cardView.getHeight() / 3) mArticle.trackImpression();
                }
                // Proceed with the current drawing pass.
                return true;
            }
        };

        // Listen to onPreDraw only if this view is potentially visible (attached to the window).
        cardView.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View v) {
                cardView.getViewTreeObserver().addOnPreDrawListener(mPreDrawObserver);
            }

            @Override
            public void onViewDetachedFromWindow(View v) {
                cardView.getViewTreeObserver().removeOnPreDrawListener(mPreDrawObserver);
            }
        });
    }

    @Override
    public void onClick(View v) {
        mNewTabPageManager.openSnippet(mArticle.mUrl);
        mArticle.trackClick();
    }

    @Override
    public void onBindViewHolder(NewTabPageListItem article) {
        mArticle = (SnippetArticle) article;

        mHeadlineTextView.setText(mArticle.mTitle);
        String publisherAttribution = String.format(PUBLISHER_FORMAT_STRING, mArticle.mPublisher,
                DateUtils.getRelativeTimeSpanString(mArticle.mPublishTimestampMilliseconds,
                        System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS));
        mPublisherTextView.setText(BidiFormatter.getInstance().unicodeWrap(publisherAttribution));

        mArticleSnippetTextView.setText(mArticle.mPreviewText);

        // If there's still a pending thumbnail fetch, cancel it.
        cancelImageFetch();

        // If the article has a thumbnail already, reuse it. Otherwise start a fetch.
        if (mArticle.getThumbnailBitmap() != null) {
            mThumbnailView.setImageBitmap(mArticle.getThumbnailBitmap());
        } else {
            mThumbnailView.setImageResource(R.drawable.ic_snippet_thumbnail_placeholder);
            mImageCallback = new FetchImageCallback(this, mArticle);
            mNewTabPageManager.fetchSnippetImage(mArticle, mImageCallback);
        }

        updateFavicon();
    }

    private static class FetchImageCallback extends Callback<Bitmap> {
        private SnippetArticleViewHolder mViewHolder;
        private final SnippetArticle mSnippet;

        public FetchImageCallback(SnippetArticleViewHolder viewHolder, SnippetArticle snippet) {
            mViewHolder = viewHolder;
            mSnippet = snippet;
        }

        @Override
        public void onResult(Bitmap image) {
            if (mViewHolder == null) return;
            mViewHolder.fadeThumbnailIn(mSnippet, image);
        }

        public void cancel() {
            // TODO(treib): Pass the "cancel" on to the actual image fetcher.
            mViewHolder = null;
        }
    }

    private void cancelImageFetch() {
        if (mImageCallback != null) {
            mImageCallback.cancel();
            mImageCallback = null;
        }
    }

    private void fadeThumbnailIn(SnippetArticle snippet, Bitmap thumbnail) {
        mImageCallback = null;
        if (thumbnail == null) return; // Nothing to do, we keep the placeholder.

        // We need to crop and scale the downloaded bitmap, as the ImageView we set it on won't be
        // able to do so when using a TransitionDrawable (as opposed to the straight bitmap).
        // That's a limitation of TransitionDrawable, which doesn't handle layers of varying sizes.
        Resources res = mThumbnailView.getResources();
        int targetSize = res.getDimensionPixelSize(R.dimen.snippets_thumbnail_size);
        Bitmap scaledThumbnail = ThumbnailUtils.extractThumbnail(
                thumbnail, targetSize, targetSize, ThumbnailUtils.OPTIONS_RECYCLE_INPUT);

        // Store the bitmap to skip the download task next time we display this snippet.
        snippet.setThumbnailBitmap(scaledThumbnail);

        // Cross-fade between the placeholder and the thumbnail.
        Drawable[] layers = {mThumbnailView.getDrawable(),
                new BitmapDrawable(mThumbnailView.getResources(), scaledThumbnail)};
        TransitionDrawable transitionDrawable = new TransitionDrawable(layers);
        mThumbnailView.setImageDrawable(transitionDrawable);
        transitionDrawable.startTransition(FADE_IN_ANIMATION_TIME_MS);
    }

    private void updateFavicon() {
        // The favicon size should match the textview height
        int widthSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        mPublisherTextView.measure(widthSpec, heightSpec);
        int faviconSizePx = mPublisherTextView.getMeasuredHeight();

        try {
            fetchFavicon(CACHE_FOR_URL, new URI(mArticle.mUrl), faviconSizePx);
        } catch (URISyntaxException e) {
            fetchFavicon(DEFAULT_FAVICON, null, faviconSizePx);
        }
    }

    private void fetchFavicon(
            @FaviconSource final int source, final URI snippetUri, final int faviconSizePx) {
        assert source == DEFAULT_FAVICON || snippetUri != null;

        switch (source) {
            case CACHE_FOR_URL:
            case CACHE_FOR_DOMAIN:
                String url = (source == CACHE_FOR_URL)
                        ? snippetUri.toString()
                        : String.format("%s://%s", snippetUri.getScheme(), snippetUri.getHost());

                mNewTabPageManager.getLocalFaviconImageForURL(
                        url, faviconSizePx, new FaviconImageCallback() {
                            @Override
                            public void onFaviconAvailable(Bitmap image, String iconUrl) {
                                if (image == null) {
                                    // Fetch again for the next source.
                                    fetchFavicon(source + 1, snippetUri, faviconSizePx);
                                    return;
                                }

                                setFaviconOnView(
                                        new BitmapDrawable(
                                                mPublisherTextView.getContext().getResources(),
                                                image),
                                        faviconSizePx);
                            }
                        });
                break;
            case DEFAULT_FAVICON:
                setFaviconOnView(ApiCompatibilityUtils.getDrawable(
                                         mPublisherTextView.getContext().getResources(),
                                         R.drawable.default_favicon),
                        faviconSizePx);
                break;
            default:
                assert false;
        }
    }

    private void setFaviconOnView(Drawable drawable, int sizePx) {
        drawable.setBounds(0, 0, sizePx, sizePx);
        ApiCompatibilityUtils.setCompoundDrawablesRelative(
                mPublisherTextView, drawable, null, null, null);
        mPublisherTextView.setVisibility(View.VISIBLE);
    }

    @Override
    public boolean isDismissable() {
        return true;
    }
}
