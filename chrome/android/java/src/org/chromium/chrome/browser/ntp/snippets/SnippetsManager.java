// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.os.Environment;
import android.support.v7.widget.RecyclerView;
import android.util.JsonReader;
import android.widget.ImageView;

import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
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

    private NewTabPageManager mNewTabPageManager;
    private SnippetsAdapter mDataAdapter;

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
        public final String mSnippet;
        public final String mUrl;
        public final String mThumbnailPath;

        private ThumbnailRenderingTask mThumbnailRenderingTask;

        // Async task to create the thumbnail from a local file
        // TODO(maybelle): This task to retrieve the thumbnail from local disk is temporary while
        // we are prototyping this feature for clank. For the real production feature, we
        // will likely have to download/decode the thumbnail on the native side.
        private static class ThumbnailRenderingTask extends AsyncTask<String, Void, Drawable> {
            private ImageView mThumbnailView;

            ThumbnailRenderingTask(ImageView thumbnailView) {
                mThumbnailView = thumbnailView;
            }

            @Override
            protected Drawable doInBackground(String... params) {
                String thumbnailPath = params[0];
                return Drawable.createFromPath(thumbnailPath);
            }

            @Override
            protected void onPostExecute(Drawable thumbnail) {
                mThumbnailView.setImageDrawable(thumbnail);
            }
        }

        public SnippetArticle(
                String title, String publisher, String snippet, String url, String thumbnailPath) {
            mTitle = title;
            mPublisher = publisher;
            mSnippet = snippet;
            mUrl = url;
            mThumbnailPath = thumbnailPath;
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

    private class ReadFileTask extends AsyncTask<Void, Void, List<SnippetListItem>> {
        @Override
        protected List<SnippetListItem> doInBackground(Void... params) {
            FileInputStream fis = null;
            try {
                fis = new FileInputStream(
                        new File(Environment.getExternalStorageDirectory().getPath()
                                + "/chrome/reading_list.json"));
                List<SnippetListItem> listSnippetsGroups = readJsonStream(fis);
                return listSnippetsGroups;
            } catch (IOException ex) {
                Log.e(TAG, "Exception reading file: %s ", ex);
            } finally {
                StreamUtil.closeQuietly(fis);
            }
            return null;
        }

        @Override
        protected void onPostExecute(List<SnippetListItem> listSnippetsGroups) {
            if (listSnippetsGroups == null) return;

            mDataAdapter.setSnippetListItems(listSnippetsGroups);
        }

        private List<SnippetListItem> readJsonStream(InputStream in) throws IOException {
            JsonReader reader = new JsonReader(new InputStreamReader(in, "UTF-8"));
            try {
                return readRecommendationsArray(reader);
            } finally {
                reader.close();
            }
        }

        private List<SnippetListItem> readRecommendationsArray(JsonReader reader)
                throws IOException {
            List<SnippetListItem> listSnippetItems = new ArrayList<SnippetListItem>();
            reader.beginArray();
            while (reader.hasNext()) {
                readSnippetGroup(listSnippetItems, reader);
            }
            reader.endArray();
            return listSnippetItems;
        }

        private void readSnippetGroup(List<SnippetListItem> listSnippetItems, JsonReader reader)
                throws IOException {
            reader.beginObject();
            while (reader.hasNext()) {
                String name = reader.nextName();
                if (name.equals("recommendation_basis")) {
                    listSnippetItems.add(new SnippetHeader(reader.nextString()));
                } else if (name.equals("articles")) {
                    readArticlesArray(listSnippetItems, reader);
                }
            }
            reader.endObject();
        }

        private void readArticlesArray(List<SnippetListItem> listSnippetItems, JsonReader reader)
                throws IOException {
            reader.beginArray();
            while (reader.hasNext()) {
                listSnippetItems.add(readArticleDetails(reader));
            }
            reader.endArray();
        }

        private SnippetArticle readArticleDetails(JsonReader reader) throws IOException {
            String headline = "";
            String publisher = "";
            String snippets = "";
            String url = "";
            String thumbnail = "";

            reader.beginObject();
            while (reader.hasNext()) {
                String name = reader.nextName();
                if (name.equals("headline")) {
                    headline = reader.nextString();
                } else if (name.equals("publisher")) {
                    publisher = reader.nextString();
                } else if (name.equals("snippet")) {
                    snippets = reader.nextString();
                } else if (name.equals("url")) {
                    url = reader.nextString();
                } else if (name.equals("thumbnail")) {
                    thumbnail = reader.nextString();
                } else {
                    reader.skipValue();
                }
            }
            reader.endObject();
            return new SnippetsManager.SnippetArticle(
                    headline, publisher, snippets, url, thumbnail);
        }
    }

    public SnippetsManager(NewTabPageManager tabManager, RecyclerView mSnippetsView) {
        mNewTabPageManager = tabManager;
        mDataAdapter = new SnippetsAdapter(this);
        mSnippetsView.setAdapter(mDataAdapter);
        new ReadFileTask().execute();
    }

    public void loadUrl(String url) {
        mNewTabPageManager.open(url);
    }
}
