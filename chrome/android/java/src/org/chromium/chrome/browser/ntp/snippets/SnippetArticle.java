// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.view.View;
import android.widget.ImageView;

import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.chrome.browser.ntp.cards.NewTabPageListItem;

import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;

/**
 * Represents the data for an article card on the NTP.
 */
public class SnippetArticle implements NewTabPageListItem {
    private static final String TAG = "SnippetArticle";

    private static final int FADE_IN_ANIMATION_TIME_MS = 300;

    public final String mTitle;
    public final String mPublisher;
    public final String mPreviewText;
    public final String mUrl;
    public final String mThumbnailUrl;
    public final long mTimestamp;
    public final int mPosition;

    private ThumbnailRenderingTask mThumbnailRenderingTask;

    // Async task to create the thumbnail from a URL
    // TODO(maybelle): This task to retrieve the thumbnail from the web is temporary while
    // we are prototyping this feature for clank. For the real production feature, we
    // will likely have to download/decode the thumbnail on the native side.
    private static class ThumbnailRenderingTask extends AsyncTask<String, Void, Drawable> {
        private ImageView mThumbnailView;

        ThumbnailRenderingTask(ImageView thumbnailView) {
            mThumbnailView = thumbnailView;

            // The view might be already holding the thumbnail from another snippet, as the view
            // is recycled. We start by hiding it to avoid having a stale image be displayed while
            // the new one is being downloaded.
            mThumbnailView.setVisibility(View.INVISIBLE);
        }

        @Override
        protected Drawable doInBackground(String... params) {
            if (params[0].isEmpty()) return null;
            InputStream is = null;
            try {
                is = (InputStream) new URL(params[0]).getContent();
                return Drawable.createFromStream(is, "thumbnail");
            } catch (MalformedURLException e) {
                Log.e(TAG, "Could not get image thumbnail due to malformed URL", e);
            } catch (IOException e) {
                Log.e(TAG, "Could not get image thumbnail", e);
            } finally {
                StreamUtil.closeQuietly(is);
            }
            return null;
        }

        @Override
        protected void onPostExecute(Drawable thumbnail) {
            if (thumbnail == null) {
                mThumbnailView.setVisibility(View.GONE);
            } else {
                // Fade in the image thumbnail
                mThumbnailView.setImageDrawable(thumbnail);
                mThumbnailView.setVisibility(View.VISIBLE);
                mThumbnailView.setAlpha(0f);
                mThumbnailView.animate().alpha(1f).setDuration(FADE_IN_ANIMATION_TIME_MS);
            }
        }
    }

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
    public int getType() {
        return NewTabPageListItem.VIEW_TYPE_SNIPPET;
    }

    /**
     * Retrieves this SnippetArticle's thumbnail asynchronously and sets it onto the given
     * ImageView.
     *
     * @param view The ImageView to set the thumbnail onto.
     */
    public void setThumbnailOnView(ImageView view) {
        if (mThumbnailRenderingTask != null) mThumbnailRenderingTask.cancel(true);

        mThumbnailRenderingTask = new ThumbnailRenderingTask(view);
        mThumbnailRenderingTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, mThumbnailUrl);
    }
}
