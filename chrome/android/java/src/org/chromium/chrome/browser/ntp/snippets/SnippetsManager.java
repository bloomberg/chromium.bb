// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.support.v7.widget.RecyclerView;
import android.widget.ImageView;

import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge.SnippetsObserver;
import org.chromium.chrome.browser.profiles.Profile;

import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;

/**
 * A class that stores and manages the article snippets that will be shown on the NTP.
 */
public class SnippetsManager {
    private static final String TAG = "SnippetsManager";

    /**
     * Describes the header of a group of similar card snippets
     */
    public static final int SNIPPET_ITEM_TYPE_HEADER = 1;
    /**
     * Describes a single card snippet
     */
    public static final int SNIPPET_ITEM_TYPE_SNIPPET = 2;

    /**
     * Enum values for recording {@code SNIPPETS_STATE_HISTOGRAM} histogram.
     * Stored here as it is recorded in multiple places.
     */
    public static final int SNIPPETS_SHOWN = 0;
    public static final int SNIPPETS_SCROLLED = 1;
    public static final int SNIPPETS_CLICKED = 2;
    public static final int NUM_SNIPPETS_ACTIONS = 3;
    public static final String SNIPPETS_STATE_HISTOGRAM = "NewTabPage.Snippets.Interactions";

    private NewTabPageManager mNewTabPageManager;
    private SnippetsAdapter mDataAdapter;
    private SnippetsBridge mSnippetsBridge;

    /** Base type for anything to add to the snippets view
     */
    public interface SnippetListItem {
        /**
         * Returns the type of this snippet item (SNIPPET_ITEM_TYPE_HEADER or
         * SNIPPET_ITEM_TYPE_SNIPPET). This is so we can distinguish between different elements
         * that are held in a single RecyclerView holder.
         *
         * @return the type of this list item.
         */
        public int getType();
    }

    /** Represents the data for a header of a group of snippets
     */
    public static class SnippetHeader implements SnippetListItem {
        public final String mRecommendationBasis;

        public SnippetHeader(String recommendationBasis) {
            mRecommendationBasis = recommendationBasis;
        }

        @Override
        public int getType() {
            return SNIPPET_ITEM_TYPE_HEADER;
        }
    }

    /**
     * Represents the data for a card snippet.
     */
    public static class SnippetArticle implements SnippetListItem {
        public final String mTitle;
        public final String mPublisher;
        public final String mPreviewText;
        public final String mUrl;
        public final String mThumbnailPath;
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
            }

            @Override
            protected Drawable doInBackground(String... params) {
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
                mThumbnailView.setImageDrawable(thumbnail);
            }
        }

        public SnippetArticle(String title, String publisher, String previewText, String url,
                String thumbnailPath, int position) {
            mTitle = title;
            mPublisher = publisher;
            mPreviewText = previewText;
            mUrl = url;
            mThumbnailPath = thumbnailPath;
            mPosition = position;
        }

        @Override
        public int getType() {
            return SNIPPET_ITEM_TYPE_SNIPPET;
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
            mThumbnailRenderingTask.executeOnExecutor(
                    AsyncTask.THREAD_POOL_EXECUTOR, mThumbnailPath);
        }
    }

    public SnippetsManager(NewTabPageManager tabManager, Profile profile) {
        mNewTabPageManager = tabManager;
        mDataAdapter = new SnippetsAdapter(this);
        mSnippetsBridge = new SnippetsBridge(profile, new SnippetsObserver() {
            @Override
            public void onSnippetsAvailable(
                    String[] titles, String[] urls, String[] thumbnailUrls, String[] previewText) {
                List<SnippetListItem> listItems = new ArrayList<SnippetListItem>();
                for (int i = 0; i < titles.length; ++i) {
                    SnippetArticle article = new SnippetArticle(
                            titles[i], "", previewText[i], urls[i], thumbnailUrls[i], i);
                    listItems.add(article);
                }
                mDataAdapter.setSnippetListItems(listItems);
            }
        });
    }

    public void setSnippetsView(RecyclerView mSnippetsView) {
        mSnippetsView.setAdapter(mDataAdapter);
    }

    public void loadUrl(String url) {
        mNewTabPageManager.open(url);
    }

    public void destroy() {
        mSnippetsBridge.destroy();
    }
}
