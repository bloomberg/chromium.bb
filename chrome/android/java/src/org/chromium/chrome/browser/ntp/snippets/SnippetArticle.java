// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.Bitmap;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.cards.NewTabPageListItem;

/**
 * Represents the data for an article card on the NTP.
 */
public class SnippetArticle implements NewTabPageListItem {
    public final String mId;
    public final String mTitle;
    public final String mPublisher;
    public final String mPreviewText;
    public final String mUrl;
    public final String mAmpUrl;
    public final String mThumbnailUrl;
    public final long mPublishTimestampMilliseconds;
    public final float mScore;
    public final int mPosition;

    /** Bitmap of the thumbnail, fetched lazily, when the RecyclerView wants to show the snippet. */
    private Bitmap mThumbnailBitmap;

    /** Stores whether impression of this article has been tracked already. */
    private boolean mImpressionTracked;

    /** Specifies ranges of positions for which we store position-specific sub-histograms. */
    private static final int[] HISTOGRAM_FOR_POSITIONS = {0, 2, 4, 9};

    /**
     * Creates a SnippetArticle object that will hold the data
     * @param title the title of the article
     * @param publisher the canonical publisher name (e.g., New York Times)
     * @param previewText the snippet preview text
     * @param url the URL of the article
     * @param mAmpUrl the AMP url for the article (possible for this to be empty)
     * @param thumbnailUrl the URL of the thumbnail
     * @param timestamp the time in ms when this article was published
     * @param score the score expressing relative quality of the article for the user
     * @param position the position of this article in the list of snippets
     */
    public SnippetArticle(String id, String title, String publisher, String previewText, String url,
            String ampUrl, String thumbnailUrl, long timestamp, float score, int position) {
        mId = id;
        mTitle = title;
        mPublisher = publisher;
        mPreviewText = previewText;
        mUrl = url;
        mAmpUrl = ampUrl;
        mThumbnailUrl = thumbnailUrl;
        mPublishTimestampMilliseconds = timestamp;
        mScore = score;
        mPosition = position;
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof SnippetArticle)) return false;
        return mId.equals(((SnippetArticle) other).mId);
    }

    @Override
    public int hashCode() {
        return mId.hashCode();
    }

    @Override
    public int getType() {
        return NewTabPageListItem.VIEW_TYPE_SNIPPET;
    }

    /**
     * Returns this article's thumbnail as a {@link Bitmap}. Can return {@code null} as it is
     * initially unset.
     */
    public Bitmap getThumbnailBitmap() {
        return mThumbnailBitmap;
    }

    /** Sets the thumbnail bitmap for this article. */
    public void setThumbnailBitmap(Bitmap bitmap) {
        mThumbnailBitmap = bitmap;
    }

    /** Tracks click on this NTP snippet in UMA. */
    public void trackClick() {
        RecordUserAction.record("MobileNTP.Snippets.Click");
        RecordHistogram.recordSparseSlowlyHistogram("NewTabPage.Snippets.CardClicked", mPosition);
        NewTabPageUma.recordSnippetAction(NewTabPageUma.SNIPPETS_ACTION_CLICKED);
        NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_SNIPPET);
        recordAgeAndScore("NewTabPage.Snippets.CardClicked");
    }

    /** Tracks impression of this NTP snippet. */
    public void trackImpression() {
        // Track UMA only upon the first impression per life-time of this object.
        if (mImpressionTracked) return;

        RecordHistogram.recordSparseSlowlyHistogram("NewTabPage.Snippets.CardShown", mPosition);
        recordAgeAndScore("NewTabPage.Snippets.CardShown");
        mImpressionTracked = true;
    }

    /** Returns whether impression of this SnippetArticle has already been tracked. */
    public boolean impressionTracked() {
        return mImpressionTracked;
    }

    private void recordAgeAndScore(String histogramPrefix) {
        // Track how the (approx.) position relates to age / score of the snippet that is clicked.
        int ageInMinutes =
                (int) ((System.currentTimeMillis() - mPublishTimestampMilliseconds) / 60000L);
        String histogramAge = histogramPrefix + "Age";
        String histogramScore = histogramPrefix + "ScoreNew";

        recordAge(histogramAge, ageInMinutes);
        recordScore(histogramScore, mScore);
        int startPosition = 0;
        for (int endPosition : HISTOGRAM_FOR_POSITIONS) {
            if (mPosition >= startPosition && mPosition <= endPosition) {
                String suffix = "_" + startPosition + "_" + endPosition;
                recordAge(histogramAge + suffix, ageInMinutes);
                recordScore(histogramScore + suffix, mScore);
                break;
            }
            startPosition = endPosition + 1;
        }
    }

    private static void recordAge(String histogramName, int ageInMinutes) {
        // Negative values (when the time of the device is set inappropriately) provide no value.
        if (ageInMinutes >= 0) {
            // If the max value below (72 hours) were to be changed, the histogram should be renamed
            // since it will change the shape of buckets.
            RecordHistogram.recordCustomCountHistogram(histogramName, ageInMinutes, 1, 72 * 60, 50);
        }
    }

    private static void recordScore(String histogramName, float score) {
        int recordedScore = Math.min((int) Math.ceil(score), 100000);
        RecordHistogram.recordCustomCountHistogram(histogramName, recordedScore, 1, 100000, 50);
    }
}
