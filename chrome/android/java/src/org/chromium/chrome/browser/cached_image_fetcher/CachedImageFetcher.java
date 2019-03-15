// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cached_image_fetcher;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Provides cached image fetching capabilities. Uses getLastUsedProfile, which
 * will need to be changed when supporting multi-profile.
 */
public interface CachedImageFetcher {
    // All UMA client names collected here to prevent duplicates.
    public static final String ASSISTANT_DETAILS_UMA_CLIENT_NAME = "AssistantDetails";
    public static final String ASSISTANT_INFO_BOX_UMA_CLIENT_NAME = "AssistantInfoBox";
    public static final String CONTEXTUAL_SUGGESTIONS_UMA_CLIENT_NAME = "ContextualSuggestions";
    public static final String FEED_UMA_CLIENT_NAME = "Feed";
    public static final String NTP_ANIMATED_LOGO_UMA_CLIENT_NAME = "NewTabPageAnimatedLogo";

    static CachedImageFetcher getInstance() {
        ThreadUtils.assertOnUiThread();
        return CachedImageFetcherImpl.getInstance();
    }

    /**
     * Report an event metric.
     *
     * @param clientName Name of the cached image fetcher client to report UMA metrics for.
     * @param eventId The event to be reported
     */
    void reportEvent(String clientName, @CachedImageFetcherEvent int eventId);

    /**
     * Fetch the gif for the given url.
     *
     * @param url The url to fetch the image from.
     * @param clientName The UMA client name to report the metrics to. If using CachedImageFetcher
     *         to fetch images and gifs, use separate clientNames for them.
     * @param callback The function which will be called when the image is ready; will be called
     *         with null result if fetching fails.
     */
    void fetchGif(String url, String clientName, Callback<BaseGifImage> callback);

    /**
     * Fetches the image at url with the desired size. Image is null if not
     * found or fails decoding.
     *
     * @param url The url to fetch the image from.
     * @param clientName Name of the cached image fetcher client to report UMA metrics for.
     * @param width The new bitmap's desired width (in pixels). If the given value is <= 0, the
     *         image won't be scaled.
     * @param height The new bitmap's desired height (in pixels). If the given value is <= 0, the
     *         image won't be scaled.
     * @param callback The function which will be called when the image is ready; will be called
     *         with null result if fetching fails;
     */
    void fetchImage(
            String url, String clientName, int width, int height, Callback<Bitmap> callback);

    /**
     * Alias of fetchImage that ignores scaling.
     *
     * @param url The url to fetch the image from.
     * @param clientName Name of the cached image fetcher client to report UMA metrics for.
     * @param callback The function which will be called when the image is ready; will be called
     *         with null result if fetching fails;
     */
    void fetchImage(String url, String clientName, Callback<Bitmap> callback);

    /**
     * Destroy method, called to clear resources to prevent leakage.
     */
    void destroy();
}
