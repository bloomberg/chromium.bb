// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import android.content.Context;
import android.support.v7.media.MediaRouter;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.media.router.cast.DiscoveryCallback;
import org.chromium.chrome.browser.media.router.cast.MediaSink;
import org.chromium.chrome.browser.media.router.cast.MediaSource;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.annotation.Nullable;

/**
 * Implements the JNI interface called from the C++ Media Router implementation on Android.
 */
@JNINamespace("media_router")
public class ChromeMediaRouter {
    private static final String TAG = "cr.MediaRouter";

    private final long mNativeMediaRouterAndroid;
    private final MediaRouter mAndroidMediaRouter;
    private Map<String, List<MediaSink>> mSinks = new HashMap<String, List<MediaSink>>();
    private final Map<String, DiscoveryCallback> mDiscoveryCallbacks =
            new HashMap<String, DiscoveryCallback>();

    /**
     * Called when the sinks found by the media route provider for
     * the particular |sourceUrn| have changed.
     * @param sourceUrn The URN of the source (presentation URL) that the sinks are received for.
     * @param sinks The list of {@link MediaSink}s
     */
    public void onSinksReceived(String sourceUrn, List<MediaSink> sinks) {
        mSinks.put(sourceUrn, sinks);
        nativeOnSinksReceived(mNativeMediaRouterAndroid, sourceUrn, sinks.size());
    }

    /**
     * Initializes the media router and its providers.
     * @param nativeMediaRouterAndroid the handler for the native counterpart of this instance
     * @param applicationContext the application context to use to obtain system APIs
     * @return an initialized {@link ChromeMediaRouter} instance
     */
    @CalledByNative
    public static ChromeMediaRouter create(long nativeMediaRouterAndroid,
            Context applicationContext) {
        return new ChromeMediaRouter(nativeMediaRouterAndroid, applicationContext);
    }

    /**
     * Starts background monitoring for available media sinks compatible with the given
     * |sourceUrn|
     * @param sourceUrn a URL to use for filtering of the available media sinks
     */
    @CalledByNative
    public void startObservingMediaSinks(String sourceUrn) {
        if (mAndroidMediaRouter == null) return;

        MediaSource source = MediaSource.from(sourceUrn);
        if (source == null) return;

        String applicationId = source.getApplicationId();
        if (mDiscoveryCallbacks.containsKey(applicationId)) {
            mDiscoveryCallbacks.get(applicationId).addSourceUrn(sourceUrn);
            return;
        }

        DiscoveryCallback callback = new DiscoveryCallback(sourceUrn, this);
        mAndroidMediaRouter.addCallback(
                source.buildRouteSelector(),
                callback,
                MediaRouter.CALLBACK_FLAG_REQUEST_DISCOVERY);
        mDiscoveryCallbacks.put(applicationId, callback);
    }

    /**
     * Stops background monitoring for available media sinks compatible with the given
     * |sourceUrn|
     * @param sourceUrn a URL passed to {@link #startObservingMediaSinks(String)} before.
     */
    @CalledByNative
    public void stopObservingMediaSinks(String sourceUrn) {
        if (mAndroidMediaRouter == null) return;

        MediaSource source = MediaSource.from(sourceUrn);
        if (source == null) return;

        String applicationId = source.getApplicationId();
        if (!mDiscoveryCallbacks.containsKey(applicationId)) return;

        DiscoveryCallback callback = mDiscoveryCallbacks.get(applicationId);
        callback.removeSourceUrn(sourceUrn);

        if (callback.isEmpty()) {
            mAndroidMediaRouter.removeCallback(callback);
            mDiscoveryCallbacks.remove(applicationId);
        }

        mSinks.remove(sourceUrn);
    }

    /**
     * Returns the URN of the media sink corresponding to the given source URN
     * and an index. Essentially a way to access the corresponding {@link MediaSink}'s
     * list via JNI.
     * @param sourceUrn The URN to get the sink for.
     * @param index The index of the sink in the current sink array.
     * @return the corresponding sink URN if found or null.
     */
    @CalledByNative
    public String getSinkUrn(String sourceUrn, int index) {
        return getSink(sourceUrn, index).getUrn();
    }

    /**
     * Returns the name of the media sink corresponding to the given source URN
     * and an index. Essentially a way to access the corresponding {@link MediaSink}'s
     * list via JNI.
     * @param sourceUrn The URN to get the sink for.
     * @param index The index of the sink in the current sink array.
     * @return the corresponding sink name if found or null.
     */
    @CalledByNative
    public String getSinkName(String sourceUrn, int index) {
        return getSink(sourceUrn, index).getName();
    }

    @VisibleForTesting
    ChromeMediaRouter(long nativeMediaRouter, Context applicationContext) {
        assert applicationContext != null;

        mNativeMediaRouterAndroid = nativeMediaRouter;
        mAndroidMediaRouter = getAndroidMediaRouter(applicationContext);
    }

    @Nullable
    private MediaRouter getAndroidMediaRouter(Context applicationContext) {
        try {
            // Pre-MR1 versions of JB do not have the complete MediaRouter APIs,
            // so getting the MediaRouter instance will throw an exception.
            return MediaRouter.getInstance(applicationContext);
        } catch (NoSuchMethodError e) {
            return null;
        }
    }

    private MediaSink getSink(String sourceUrn, int index) {
        assert mSinks.containsKey(sourceUrn);
        return mSinks.get(sourceUrn).get(index);
    }

    native void nativeOnSinksReceived(
            long nativeMediaRouterAndroid, String sourceUrn, int count);
}