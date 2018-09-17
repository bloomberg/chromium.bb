// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;

import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.host.imageloader.ImageLoaderApi;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.feed.FeedImageLoaderBridge.ImageResponse;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ThumbnailGradient;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * Provides image loading and other host-specific asset fetches for Feed.
 */
public class FeedImageLoader implements ImageLoaderApi {
    private static final String ASSET_PREFIX = "asset://";
    private static final String DRAWABLE_RESOURCE_TYPE = "drawable";
    private static final String OVERLAY_IMAGE_PREFIX = "overlay-image://";
    private static final String OVERLAY_IMAGE_URL_PARAM = "url";
    private static final String OVERLAY_IMAGE_DIRECTION_PARAM = "direction";
    private static final String OVERLAY_IMAGE_DIRECTION_START = "start";
    private static final String OVERLAY_IMAGE_DIRECTION_END = "end";

    private FeedImageLoaderBridge mFeedImageLoaderBridge;
    private Context mActivityContext;

    /**
     * Creates a FeedImageLoader for fetching image for the current user.
     *
     * @param profile Profile of the user we are rendering the Feed for.
     * @param activityContext Context of the user we are rendering the Feed for.
     */
    public FeedImageLoader(Profile profile, Context activityContext) {
        this(profile, activityContext, new FeedImageLoaderBridge());
    }

    /**
     * Creates a FeedImageLoader for fetching image for the current user.
     *
     * @param profile Profile of the user we are rendering the Feed for.
     * @param activityContext Context of the user we are rendering the Feed for.
     * @param bridge The FeedImageLoaderBridge implementation can handle fetching image request.
     */
    public FeedImageLoader(Profile profile, Context activityContext, FeedImageLoaderBridge bridge) {
        mFeedImageLoaderBridge = bridge;
        mFeedImageLoaderBridge.init(profile);
        mActivityContext = activityContext;
    }

    @Override
    public void loadDrawable(List<String> urls, Consumer<Drawable> consumer) {
        assert mFeedImageLoaderBridge != null;
        List<String> assetUrls = new ArrayList<>();
        List<String> networkUrls = new ArrayList<>();

        // Maps position in the networkUrls list to overlay image direction.
        HashMap<Integer, Integer> overlayImages = new HashMap<>();

        // Since loading APK resource("asset://"") only can be done in Java side, we filter out
        // asset urls, and pass the other URLs to C++ side. This will change the order of |urls|,
        // because we will process asset:// URLs after network URLs, but once
        // https://crbug.com/840578 resolved, we can process URLs ordering same as |urls|.
        for (String url : urls) {
            if (url.startsWith(ASSET_PREFIX)) {
                assetUrls.add(url);
            } else if (url.startsWith(OVERLAY_IMAGE_PREFIX)) {
                Uri uri = Uri.parse(url);

                String sourceUrl = uri.getQueryParameter(OVERLAY_IMAGE_URL_PARAM);
                if (!TextUtils.isEmpty(sourceUrl)) {
                    networkUrls.add(sourceUrl);
                    addOverlayDirectionToMap(overlayImages, networkUrls.size() - 1, uri);
                } else {
                    assert false : "Overlay image source URL empty";
                }
            } else {
                // Assume this is a regular web image.
                networkUrls.add(url);
            }
        }

        if (networkUrls.size() == 0) {
            Drawable drawable = getAssetDrawable(assetUrls);
            consumer.accept(drawable);
            return;
        }

        mFeedImageLoaderBridge.fetchImage(networkUrls, new Callback<ImageResponse>() {
            @Override
            public void onResult(ImageResponse response) {
                if (response.bitmap != null) {
                    Drawable drawable;
                    if (overlayImages.containsKey(response.imagePositionInList)) {
                        drawable = ThumbnailGradient.createDrawableWithGradientIfNeeded(
                                response.bitmap, overlayImages.get(response.imagePositionInList),
                                mActivityContext.getResources());
                    } else {
                        drawable = new BitmapDrawable(
                                mActivityContext.getResources(), response.bitmap);
                    }

                    consumer.accept(drawable);
                    return;
                }

                // Since no image was available for downloading over the network, attempt to load a
                // drawable locally.
                Drawable drawable = getAssetDrawable(assetUrls);
                consumer.accept(drawable);
            }
        });
    }

    /** Cleans up FeedImageLoaderBridge. */
    public void destroy() {
        assert mFeedImageLoaderBridge != null;

        mFeedImageLoaderBridge.destroy();
        mFeedImageLoaderBridge = null;
    }

    private Drawable getAssetDrawable(List<String> assetUrls) {
        for (String url : assetUrls) {
            String resourceName = url.substring(ASSET_PREFIX.length());
            int resourceId = mActivityContext.getResources().getIdentifier(
                    resourceName, DRAWABLE_RESOURCE_TYPE, mActivityContext.getPackageName());
            if (resourceId != 0) {
                Drawable drawable = AppCompatResources.getDrawable(mActivityContext, resourceId);
                if (drawable != null) {
                    return drawable;
                }
            }
        }
        return null;
    }

    /**
     * Determine where the thumbnail is located in the card using the "direction" param and add it
     * to the provided HashMap.
     * @param overlayImageMap The HashMap used to store the overlay direction.
     * @param key The key for the overlay image.
     * @param overlayImageUri The URI for the overlay image.
     */
    private void addOverlayDirectionToMap(
            HashMap<Integer, Integer> overlayImageMap, int key, Uri overlayImageUri) {
        String direction = overlayImageUri.getQueryParameter(OVERLAY_IMAGE_DIRECTION_PARAM);
        if (TextUtils.equals(direction, OVERLAY_IMAGE_DIRECTION_START)) {
            overlayImageMap.put(key, ThumbnailGradient.ThumbnailLocation.START);
        } else if (TextUtils.equals(direction, OVERLAY_IMAGE_DIRECTION_END)) {
            overlayImageMap.put(key, ThumbnailGradient.ThumbnailLocation.END);
        } else {
            assert false : "Overlay image direction must be either start or end";
        }
    }
}
