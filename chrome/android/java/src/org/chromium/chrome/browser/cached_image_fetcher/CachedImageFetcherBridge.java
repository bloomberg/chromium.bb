// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cached_image_fetcher;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Provides access to native implementations of CachedImageFetcher for the given profile.
 */
@JNINamespace("image_fetcher")
class CachedImageFetcherBridge {
    private long mNativeCachedImageFetcherBridge;

    /**
     * Creates a CachedImageFetcherBridge for accessing the native CachedImageFetcher
     * implementation.
     */
    public CachedImageFetcherBridge(Profile profile) {
        mNativeCachedImageFetcherBridge = nativeInit(profile);
    }

    /** Cleans up native half of bridge. */
    public void destroy() {
        assert mNativeCachedImageFetcherBridge != 0;
        nativeDestroy(mNativeCachedImageFetcherBridge);
        mNativeCachedImageFetcherBridge = 0;
    }

    /**
     * Get the full path of the given url on disk.
     *
     * @param url The url to hash.
     * @return The full path to the resource on disk.
     */
    public String getFilePath(String url) {
        assert mNativeCachedImageFetcherBridge != 0;
        return nativeGetFilePath(mNativeCachedImageFetcherBridge, url);
    }

    /**
     * Fetch a gif from native or null if the gif can't be fetched or decoded.
     *
     * @param url The url to fetch.
     * @param clientName The UMA client name to report the metrics to.
     * @param callback The callback to call when the gif is ready.
     */
    public void fetchGif(String url, String clientName, Callback<BaseGifImage> callback) {
        assert mNativeCachedImageFetcherBridge != 0;
        nativeFetchImageData(mNativeCachedImageFetcherBridge, url, clientName, (byte[] data) -> {
            if (data == null || data.length == 0) {
                callback.onResult(null);
            }

            callback.onResult(new BaseGifImage(data));
        });
    }

    /**
     * Fetch the image from native.
     *
     * @param url The url to fetch.
     * @param clientName The UMA client name to report the metrics to.
     * @param callback The callback to call when the image is ready.
     */
    public void fetchImage(String url, String clientName, Callback<Bitmap> callback) {
        assert mNativeCachedImageFetcherBridge != 0;
        nativeFetchImage(mNativeCachedImageFetcherBridge, url, clientName, callback);
    }

    /**
     * Report a metrics event.
     *
     * @param clientName The UMA client name to report the metrics to.
     * @param eventId The event to report.
     */
    public void reportEvent(String clientName, @CachedImageFetcherEvent int eventId) {
        assert mNativeCachedImageFetcherBridge != 0;
        nativeReportEvent(mNativeCachedImageFetcherBridge, clientName, eventId);
    }

    /**
     * Report a timing event for a cache hit.
     *
     * @param clientName The UMA client name to report the metrics to.
     * @param startTimeMillis The start time (in milliseconds) of the request, used to measure the
     * total duration.
     */
    public void reportCacheHitTime(String clientName, long startTimeMillis) {
        assert mNativeCachedImageFetcherBridge != 0;
        nativeReportCacheHitTime(mNativeCachedImageFetcherBridge, clientName, startTimeMillis);
    }

    /**
     * Report a timing event for a call to native
     *
     * @param clientName The UMA client name to report the metrics to.
     * @param startTimeMillis The start time (in milliseconds) of the request, used to measure the
     * total duration.
     */
    public void reportTotalFetchTimeFromNative(String clientName, long startTimeMillis) {
        assert mNativeCachedImageFetcherBridge != 0;
        nativeReportTotalFetchTimeFromNative(
                mNativeCachedImageFetcherBridge, clientName, startTimeMillis);
    }

    // Native methods
    private static native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeCachedImageFetcherBridge);
    private native String nativeGetFilePath(long nativeCachedImageFetcherBridge, String url);
    private native void nativeFetchImageData(long nativeCachedImageFetcherBridge, String url,
            String clientName, Callback<byte[]> callback);
    private native void nativeFetchImage(long nativeCachedImageFetcherBridge, String url,
            String clientName, Callback<Bitmap> callback);
    private native void nativeReportEvent(
            long nativeCachedImageFetcherBridge, String clientName, int eventId);
    private native void nativeReportCacheHitTime(
            long nativeCachedImageFetcherBridge, String clientName, long startTimeMillis);
    private native void nativeReportTotalFetchTimeFromNative(
            long nativeCachedImageFetcherBridge, String clientName, long startTimeMillis);
}
