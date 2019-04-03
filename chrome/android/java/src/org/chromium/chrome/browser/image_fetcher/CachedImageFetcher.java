// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_fetcher;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * ImageFetcher implementation that uses a disk cache.
 */
public class CachedImageFetcher extends ImageFetcher {
    private static final String TAG = "CachedImageFetcher";

    // The native bridge.
    private ImageFetcherBridge mImageFetcherBridge;

    /**
     * Creates a CachedImageFetcher with the given bridge.
     *
     * @param bridge Bridge used to interact with native.
     */
    protected CachedImageFetcher(ImageFetcherBridge bridge) {
        mImageFetcherBridge = bridge;
    }

    @Override
    public void reportEvent(String clientName, @CachedImageFetcherEvent int eventId) {
        mImageFetcherBridge.reportEvent(clientName, eventId);
    }

    @Override
    public void destroy() {
        // Do nothing, this lives for the lifetime of the application.
    }

    @Override
    public void fetchGif(String url, String clientName, Callback<BaseGifImage> callback) {
        long startTimeMillis = System.currentTimeMillis();
        String filePath = mImageFetcherBridge.getFilePath(url);
        // TODO(crbug.com/947176): Use the new PostTask api for deferred tasks.
        new AsyncTask<BaseGifImage>() {
            @Override
            protected BaseGifImage doInBackground() {
                return tryToLoadGifFromDisk(filePath);
            }

            @Override
            protected void onPostExecute(BaseGifImage gif) {
                if (gif != null) {
                    callback.onResult(gif);
                    reportEvent(clientName, CachedImageFetcherEvent.JAVA_DISK_CACHE_HIT);
                    mImageFetcherBridge.reportCacheHitTime(clientName, startTimeMillis);
                } else {
                    mImageFetcherBridge.fetchGif(url, clientName, (BaseGifImage gifFromNative) -> {
                        callback.onResult(gifFromNative);
                        mImageFetcherBridge.reportTotalFetchTimeFromNative(
                                clientName, startTimeMillis);
                    });
                }
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @Override
    public void fetchImage(
            String url, String clientName, int width, int height, Callback<Bitmap> callback) {
        fetchImageImpl(url, clientName, width, height, callback);
    }

    @Override
    public @ImageFetcherConfig int getConfig() {
        return ImageFetcherConfig.DISK_CACHE_ONLY;
    }

    /**
     * Starts an AsyncTask to first check the disk for the desired image, then fetches from the
     * network if it isn't found.
     *
     * @param url The url to fetch the image from.
     * @param width The new bitmap's desired width (in pixels).
     * @param height The new bitmap's desired height (in pixels).
     * @param callback The function which will be called when the image is ready.
     */
    @VisibleForTesting
    void fetchImageImpl(
            String url, String clientName, int width, int height, Callback<Bitmap> callback) {
        long startTimeMillis = System.currentTimeMillis();
        String filePath = mImageFetcherBridge.getFilePath(url);
        // TODO(crbug.com/947176): Use the new PostTask api for deferred tasks.
        new AsyncTask<Bitmap>() {
            @Override
            protected Bitmap doInBackground() {
                return tryToLoadImageFromDisk(filePath);
            }

            @Override
            protected void onPostExecute(Bitmap bitmap) {
                if (bitmap != null) {
                    callback.onResult(bitmap);
                    reportEvent(clientName, CachedImageFetcherEvent.JAVA_DISK_CACHE_HIT);
                    mImageFetcherBridge.reportCacheHitTime(clientName, startTimeMillis);
                } else {
                    mImageFetcherBridge.fetchImage(getConfig(), url, clientName, width, height,
                            (Bitmap bitmapFromNative) -> {
                                callback.onResult(bitmapFromNative);
                                mImageFetcherBridge.reportTotalFetchTimeFromNative(
                                        clientName, startTimeMillis);
                            });
                }
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /** Wrapper function to decode a file for disk, useful for testing. */
    @VisibleForTesting
    Bitmap tryToLoadImageFromDisk(String filePath) {
        if (new File(filePath).exists()) {
            return BitmapFactory.decodeFile(filePath, null);
        } else {
            return null;
        }
    }

    @VisibleForTesting
    BaseGifImage tryToLoadGifFromDisk(String filePath) {
        try {
            File file = new File(filePath);
            byte[] fileBytes = new byte[(int) file.length()];
            FileInputStream fileInputStream = new FileInputStream(filePath);

            int bytesRead = fileInputStream.read(fileBytes);
            if (bytesRead != fileBytes.length) return null;

            return new BaseGifImage(fileBytes);
        } catch (IOException e) {
            Log.w(TAG, "Failed to read: %s", filePath, e);
            return null;
        }
    }
}
