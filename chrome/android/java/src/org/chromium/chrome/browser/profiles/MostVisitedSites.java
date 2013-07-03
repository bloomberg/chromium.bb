// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import android.graphics.Bitmap;

import org.chromium.base.CalledByNative;

/**
 * Methods to bridge into native history to provide most recent urls, titles and thumbnails.
 */
public class MostVisitedSites {
    private Profile mProfile;

    /**
     * Interface for callback object for fetching most visisted urls.
     */
    public interface MostVisitedURLsCallback {
        /**
         * Callback method for fetching most visited URLs.
         * Parameters guaranteed to be non-null.
         *
         * @param titles Array of most visited url page titles.
         * @param urls Array of most visited urls.
         */
        @CalledByNative("MostVisitedURLsCallback")
        public void onMostVisitedURLsAvailable(String[] titles, String[] urls);
    }

    public interface ThumbnailCallback {
        /**
         * Callback method for fetching thumbnail of a most visited URL.
         * Parameter may be null.
         *
         * @param Bitmap thumbnail Bitmap.
         */
        @CalledByNative("ThumbnailCallback")
        public void onMostVisitedURLsThumbnailAvailable(Bitmap thumbnail);
    }

    /**
     * MostVisitedSites constructor requires a valid user profile object.
     *
     * @param profile A valid user profile object.
     */
    public MostVisitedSites(Profile profile) {
        mProfile = profile;
    }

    /**
     * Asynchronous method that fetches most visited urls and their page titles.
     *
     * @param callback Instance of a callback object.
     * @param numResults Maximum number of results to return.
     */
    public void getMostVisitedURLs(MostVisitedURLsCallback callback, int numResults) {
        nativeGetMostVisitedURLs(mProfile, callback, numResults);
    }

    /**
     * Fetches thumbnail bitmap for a url returned by getMostVisitedURLs.
     *
     * @param url String representation of url.
     * @param callback Instance of a callback object.
     * @return Bitmap Thumbnail image for specified url.
     */
    public void getURLThumbnail(String url, ThumbnailCallback callback) {
        nativeGetURLThumbnail(mProfile, url, callback);
    }

    private static native void nativeGetMostVisitedURLs(
            Profile profile, MostVisitedURLsCallback callback, int numResults);
    private static native void nativeGetURLThumbnail(
            Profile profile, String url, ThumbnailCallback callback);
}
