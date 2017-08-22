// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.graphics.Bitmap;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.util.LruCache;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;

import java.lang.ref.WeakReference;
import java.util.ArrayDeque;
import java.util.Deque;

/**
 * Concrete implementation of {@link ThumbnailProvider}.
 *
 * Thumbnails are cached in memory and shared across all ThumbnailProviderImpls. Missing thumbnails
 * are retrieved by {@link ThumbnailDiskStorage}. The memory cache is LRU, limited in size, and
 * automatically garbage collected under memory pressure.
 *
 * A queue of requests is maintained in FIFO order.
 *
 * TODO(dfalcantara): Figure out how to send requests simultaneously to the utility process without
 *                    duplicating work to decode the same image for two different requests.
 */
public class ThumbnailProviderImpl implements ThumbnailProvider, ThumbnailStorageDelegate {
    /** 5 MB of thumbnails should be enough for everyone. */
    private static final int MAX_CACHE_BYTES = 5 * 1024 * 1024;

    /**
     *  Weakly referenced cache containing thumbnails that can be deleted under memory pressure.
     *  Key in the cache is a pair of the content ID and the requested size (maximum dimension of
     *  the smaller side) of the thumbnail. Value is a pair of the thumbnail and its byte size.
     * */
    private static WeakReference<LruCache<Pair<String, Integer>, Pair<Bitmap, Integer>>>
            sBitmapCache = new WeakReference<>(null);

    /** Enqueues requests. */
    private final Handler mHandler;

    /** Queue of files to retrieve thumbnails for. */
    private final Deque<ThumbnailRequest> mRequestQueue;

    /** Request that is currently having its thumbnail retrieved. */
    private ThumbnailRequest mCurrentRequest;

    private ThumbnailDiskStorage mStorage;

    public ThumbnailProviderImpl() {
        mHandler = new Handler(Looper.getMainLooper());
        mRequestQueue = new ArrayDeque<>();
        mStorage = ThumbnailDiskStorage.create(this);
    }

    /** Destroys the class and clears the request queue. */
    @Override
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        mRequestQueue.clear();
        mStorage.destroy();
    }

    /**
     * The returned bitmap will have at least one of its dimensions smaller than or equal to the
     * size specified in the request. Requests with no file path or content ID will not be
     * processed.
     *
     * @param request Parameters that describe the thumbnail being retrieved.
     */
    @Override
    public void getThumbnail(ThumbnailRequest request) {
        if (TextUtils.isEmpty(request.getFilePath()) || TextUtils.isEmpty(request.getContentId())) {
            return;
        }

        Bitmap cachedBitmap = getBitmapFromCache(request.getContentId(), request.getIconSize());
        if (cachedBitmap != null) {
            request.onThumbnailRetrieved(request.getContentId(), cachedBitmap);
            return;
        }

        mRequestQueue.offer(request);
        processQueue();
    }

    /** Removes a particular file from the pending queue. */
    @Override
    public void cancelRetrieval(ThumbnailRequest request) {
        if (mRequestQueue.contains(request)) mRequestQueue.remove(request);
    }

    /**
     * Removes the thumbnails (different sizes) with {@code contentId} from disk.
     * @param contentId The content ID of the thumbnail to remove.
     */
    @Override
    public void removeThumbnailsFromDisk(String contentId) {
        mStorage.removeFromDisk(contentId);
    }

    private void processQueue() {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                processNextRequest();
            }
        });
    }

    private Bitmap getBitmapFromCache(String contentId, int bitmapSizePx) {
        Pair<Bitmap, Integer> cachedPair =
                getBitmapCache().get(Pair.create(contentId, bitmapSizePx));
        if (cachedPair == null) return null;
        Bitmap cachedBitmap = cachedPair.first;

        if (cachedBitmap == null) return null;
        assert !cachedBitmap.isRecycled();
        return cachedBitmap;
    }

    private void processNextRequest() {
        if (mCurrentRequest != null || mRequestQueue.isEmpty()) return;

        mCurrentRequest = mRequestQueue.poll();

        Bitmap cachedBitmap =
                getBitmapFromCache(mCurrentRequest.getContentId(), mCurrentRequest.getIconSize());
        if (cachedBitmap == null) {
            mStorage.retrieveThumbnail(mCurrentRequest);
        } else {
            // Send back the already-processed file.
            onThumbnailRetrieved(mCurrentRequest.getContentId(), cachedBitmap);
        }
    }

    /**
     * Called when thumbnail is ready, retrieved from memory cache or by
     * {@link ThumbnailDiskStorage}.
     * @param contentId Content ID for the thumbnail retrieved.
     * @param bitmap The thumbnail retrieved.
     */
    @Override
    public void onThumbnailRetrieved(@NonNull String contentId, @Nullable Bitmap bitmap) {
        if (bitmap != null) {
            // We set the key pair to contain the required size (maximum dimension (pixel) of the
            // smaller side) instead of the minimal dimension of the thumbnail so that future
            // fetches of this thumbnail can recognise the key in the cache.
            if (!SysUtils.isLowEndDevice()) {
                getBitmapCache().put(Pair.create(contentId, mCurrentRequest.getIconSize()),
                        Pair.create(bitmap, bitmap.getByteCount()));
            }
            mCurrentRequest.onThumbnailRetrieved(contentId, bitmap);
        }

        mCurrentRequest = null;
        processQueue();
    }

    private static LruCache<Pair<String, Integer>, Pair<Bitmap, Integer>> getBitmapCache() {
        ThreadUtils.assertOnUiThread();

        LruCache<Pair<String, Integer>, Pair<Bitmap, Integer>> cache =
                sBitmapCache == null ? null : sBitmapCache.get();
        if (cache != null) return cache;

        // Create a new weakly-referenced cache.
        cache = new LruCache<Pair<String, Integer>, Pair<Bitmap, Integer>>(MAX_CACHE_BYTES) {
            @Override
            protected int sizeOf(
                    Pair<String, Integer> thumbnailIdPair, Pair<Bitmap, Integer> thumbnail) {
                return thumbnail == null ? 0 : thumbnail.second;
            }
        };
        sBitmapCache = new WeakReference<>(cache);
        return cache;
    }

    /**
     * Evicts all cached thumbnails from previous fetches.
     */
    public static void clearCache() {
        getBitmapCache().evictAll();
    }
}
