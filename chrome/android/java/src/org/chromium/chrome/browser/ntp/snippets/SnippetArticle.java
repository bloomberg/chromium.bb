// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.Bitmap;

import org.chromium.chrome.browser.ntp.cards.NewTabPageListItem;

/**
 * Represents the data for an article card on the NTP.
 */
public class SnippetArticle implements NewTabPageListItem {

    public final String mTitle;
    public final String mPublisher;
    public final String mPreviewText;
    public final String mUrl;
    public final String mThumbnailUrl;
    public final long mTimestamp;
    public final int mPosition;

    /** Bitmap of the thumbnail, fetched lazily, when the RecyclerView wants to show the snippet. */
    private Bitmap mThumbnailBitmap;

    /**
     * Creates a SnippetArticle object that will hold the data
     * @param title the title of the article
     * @param publisher the canonical publisher name (e.g., New York Times)
     * @param previewText the snippet preview text
     * @param url the URL of the article
     * @param thumbnailUrl the URL of the thumbnail
     * @param timestamp the time in ms when this article was published
     * @param position the position of this article in the list of snippets
     */
    public SnippetArticle(String title, String publisher, String previewText, String url,
            String thumbnailUrl, long timestamp, int position) {
        mTitle = title;
        mPublisher = publisher;
        mPreviewText = previewText;
        mUrl = url;
        mThumbnailUrl = thumbnailUrl;
        mTimestamp = timestamp;
        mPosition = position;
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof SnippetArticle)) return false;
        return mUrl.equals(((SnippetArticle) other).mUrl);
    }

    @Override
    public int hashCode() {
        return mUrl.hashCode();
    }

    @Override
    public int getType() {
        return NewTabPageListItem.VIEW_TYPE_SNIPPET;
    }

    /**
     * Returns this article's tumbnail as a {@link Bitmap}. Can return {@code null} as it is
     * initially unset.
     */
    public Bitmap getThumbnailBitmap() {
        return mThumbnailBitmap;
    }

    /** Sets the tumbnail bitmap for this article. */
    public void setThumbnailBitmap(Bitmap bitmap) {
        mThumbnailBitmap = bitmap;
    }
}
