// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cached_image_fetcher;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.profiles.Profile;
/**
 * Provides cached image fetching capabilities. Uses getLastUsedProfile, which
 * will need to be changed when supporting multi-profile.
 */
public class CachedImageFetcher {
    private static CachedImageFetcher sInstance;

    public static CachedImageFetcher getInstance() {
        ThreadUtils.assertOnUiThread();

        if (sInstance == null) {
            sInstance = new CachedImageFetcher(Profile.getLastUsedProfile());
        }

        return sInstance;
    }

    private CachedImageFetcherBridge mCachedImageFetcherBridge;

    /**
     * Creates a CachedImageFetcher for the current user.
     *
     * @param profile Profile of the user we are fetching for.
     */
    private CachedImageFetcher(Profile profile) {
        this(new CachedImageFetcherBridge(profile));
    }

    /**
     * Creates a CachedImageFetcher for testing.
     *
     * @param bridge Mock bridge to use.
     */
    @VisibleForTesting
    CachedImageFetcher(CachedImageFetcherBridge bridge) {
        mCachedImageFetcherBridge = bridge;
    }

    /**
     * Fetches the image at url with the desired size. Image is null if not
     * found or fails decoding.
     *
     * @param url The url to fetch the image from.
     * @param width The new bitmap's desired width (in pixels).
     * @param height The new bitmap's desired height (in pixels).
     * @param callback The function which will be called when the image is ready.
     */
    public void fetchImage(String url, int width, int height, Callback<Bitmap> callback) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.outWidth = width;
        options.outHeight = height;

        fetchImage(url, options, callback);
    }

    /**
     * Alias of fetchImage that ignores scaling.
     *
     * @param url The url to fetch the image from.
     * @param callback The function which will be called when the image is ready.
     */
    public void fetchImage(String url, Callback<Bitmap> callback) {
        fetchImage(url, new BitmapFactory.Options(), callback);
    }

    /**
     * Starts an AsyncTask to first check the disk for the desired image, then
     * fetches from the network if it isn't found.
     *
     * @param url The url to fetch the image from.
     * @param options Settings when loading the image.
     * @param callback The function which will be called when the image is ready.
     */
    @VisibleForTesting
    protected void fetchImage(
            String url, BitmapFactory.Options options, Callback<Bitmap> callback) {
        String filePath = mCachedImageFetcherBridge.getFilePath(url);
        new AsyncTask<Bitmap>() {
            @Override
            protected Bitmap doInBackground() {
                return tryToLoadImageFromDisk(filePath, options);
            }

            @Override
            protected void onPostExecute(Bitmap bitmap) {
                if (bitmap != null) {
                    callback.onResult(bitmap);
                    return;
                }

                mCachedImageFetcherBridge.fetchImage(
                        url, options.outWidth, options.outHeight, callback);
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /** Wrapper function to decode a file for disk, useful for testing. */
    @VisibleForTesting
    Bitmap tryToLoadImageFromDisk(String filePath, BitmapFactory.Options options) {
        return BitmapFactory.decodeFile(filePath, options);
    }
}
