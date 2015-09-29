// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import android.content.Context;
import android.support.v7.media.MediaRouter;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.media.router.cast.CastMediaRouteProvider;
import org.chromium.chrome.browser.media.router.cast.MediaSink;
import org.chromium.chrome.browser.media.router.cast.MediaSource;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.annotation.Nullable;

/**
 * Implements the JNI interface called from the C++ Media Router implementation on Android.
 * Owns a list of {@link MediaRouteProvider} implementations and dispatches native calls to them.
 */
@JNINamespace("media_router")
public class ChromeMediaRouter implements MediaRouteManager {
    private static final String TAG = "cr.MediaRouter";

    private final long mNativeMediaRouterAndroid;
    private final List<MediaRouteProvider> mRouteProviders = new ArrayList<MediaRouteProvider>();
    private final Map<String, MediaRouteProvider> mSinkIdsToProviders =
            new HashMap<String, MediaRouteProvider>();
    private final Map<String, MediaRouteProvider> mRouteIdsToProviders =
            new HashMap<String, MediaRouteProvider>();
    private final Map<String, Map<MediaRouteProvider, List<MediaSink>>> mSinksPerSourcePerProvider =
            new HashMap<String, Map<MediaRouteProvider, List<MediaSink>>>();
    private final Map<String, List<MediaSink>> mSinksPerSource =
            new HashMap<String, List<MediaSink>>();


    /**
     * Obtains the {@link MediaRouter} instance given the application context.
     * @param applicationContext The context to get the Android media router service for.
     * @return Null if the media router API is not supported, the service instance otherwise.
     */
    @Nullable
    public static MediaRouter getAndroidMediaRouter(Context applicationContext) {
        try {
            // Pre-MR1 versions of JB do not have the complete MediaRouter APIs,
            // so getting the MediaRouter instance will throw an exception.
            return MediaRouter.getInstance(applicationContext);
        } catch (NoSuchMethodError e) {
            return null;
        }
    }

    /**
     * @param presentationId the presentation id associated with the route
     * @param sinkId the id of the {@link MediaSink} associated with the route
     * @param sourceUrn the presentation URL associated with the route
     * @return the media route id corresponding to the given parameters.
     */
    public static String createMediaRouteId(
            String presentationId, String sinkId, String sourceUrn) {
        return String.format("route:%s/%s/%s", presentationId, sinkId, sourceUrn);
    }

    @Override
    public void onSinksReceived(
            String sourceId, MediaRouteProvider provider, List<MediaSink> sinks) {
        if (!mSinksPerSourcePerProvider.containsKey(sourceId)) {
            mSinksPerSourcePerProvider.put(
                    sourceId, new HashMap<MediaRouteProvider, List<MediaSink>>());
        }

        // Replace the sinks found by this provider with the new list.
        Map<MediaRouteProvider, List<MediaSink>> sinksPerProvider =
                mSinksPerSourcePerProvider.get(sourceId);
        sinksPerProvider.put(provider, sinks);

        List<MediaSink> allSinksPerSource = new ArrayList<MediaSink>();
        for (List<MediaSink> s : sinksPerProvider.values()) allSinksPerSource.addAll(s);

        mSinksPerSource.put(sourceId, allSinksPerSource);
        nativeOnSinksReceived(mNativeMediaRouterAndroid, sourceId, allSinksPerSource.size());
    }

    @Override
    public void onRouteCreated(
            String mediaRouteId, int requestId, MediaRouteProvider provider, boolean wasLaunched) {
        mRouteIdsToProviders.put(mediaRouteId, provider);
        nativeOnRouteCreated(mNativeMediaRouterAndroid, mediaRouteId, requestId, wasLaunched);
    }

    @Override
    public void onRouteCreationError(String errorText, int requestId) {
        nativeOnRouteCreationError(mNativeMediaRouterAndroid, errorText, requestId);
    }

    @Override
    public void onRouteClosed(String mediaRouteId) {
        nativeOnRouteClosed(mNativeMediaRouterAndroid, mediaRouteId);
    }

    @Override
    public void onMessageSentResult(boolean success, int callbackId) {
        nativeOnMessageSentResult(mNativeMediaRouterAndroid, success, callbackId);
    }

    @Override
    public void onMessage(String mediaRouteId, String message) {
        nativeOnMessage(mNativeMediaRouterAndroid, mediaRouteId, message);
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
     * @param sourceId a URL to use for filtering of the available media sinks
     */
    @CalledByNative
    public void startObservingMediaSinks(String sourceId) {
        for (MediaRouteProvider provider : mRouteProviders) {
            provider.startObservingMediaSinks(sourceId);
        }
    }

    /**
     * Stops background monitoring for available media sinks compatible with the given
     * |sourceUrn|
     * @param sourceId a URL passed to {@link #startObservingMediaSinks(String)} before.
     */
    @CalledByNative
    public void stopObservingMediaSinks(String sourceId) {
        for (MediaRouteProvider provider : mRouteProviders) {
            provider.stopObservingMediaSinks(sourceId);
        }
        mSinksPerSource.remove(sourceId);
        mSinksPerSourcePerProvider.remove(sourceId);
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

    /**
     * Initiates route creation with the given parameters. Notifies the native client of success
     * and failure.
     * @param sourceId the id of the {@link MediaSource} to route to the sink.
     * @param sinkId the id of the {@link MediaSink} to route the source to.
     * @param presentationId the id of the presentation to be used by the page.
     * @param requestId the id of the route creation request tracked by the native side.
     */
    @CalledByNative
    public void createRoute(
            String sourceId,
            String sinkId,
            String presentationId,
            int requestId) {
        MediaRouteProvider provider = getProviderForSourceAndSink(sourceId, sinkId);
        assert provider != null;

        String routeId = createMediaRouteId(presentationId, sinkId, sourceId);
        provider.createRoute(sourceId, sinkId, routeId, requestId);
    }

    /**
     * Closes the route specified by the id.
     * @param routeId the id of the route to close.
     */
    @CalledByNative
    public void closeRoute(String routeId) {
        MediaRouteProvider provider = mRouteIdsToProviders.get(routeId);
        assert provider != null;

        provider.closeRoute(routeId);
    }

    /**
     * Sends a string message to the specified route.
     * @param routeId The id of the route to send the message to.
     * @param message The message to send.
     * @param callbackId The id of the result callback tracked by the native side.
     */
    @CalledByNative
    public void sendStringMessage(String routeId, String message, int callbackId) {
        MediaRouteProvider provider = mRouteIdsToProviders.get(routeId);
        assert provider != null;

        provider.sendStringMessage(routeId, message, callbackId);
    }

    @VisibleForTesting
    ChromeMediaRouter(long nativeMediaRouter, Context applicationContext) {
        MediaRouteProvider provider = CastMediaRouteProvider.create(applicationContext, this);
        if (provider != null) mRouteProviders.add(provider);

        mNativeMediaRouterAndroid = nativeMediaRouter;
    }

    private MediaSink getSink(String sourceId, int index) {
        assert mSinksPerSource.containsKey(sourceId);
        return mSinksPerSource.get(sourceId).get(index);
    }

    private MediaRouteProvider getProviderForSourceAndSink(String sourceId, String sinkId) {
        assert mSinksPerSourcePerProvider.containsKey(sourceId);
        Map<MediaRouteProvider, List<MediaSink>> sinksPerProvider =
                mSinksPerSourcePerProvider.get(sourceId);
        for (Map.Entry<MediaRouteProvider, List<MediaSink>> entry : sinksPerProvider.entrySet()) {
            for (MediaSink sink : entry.getValue()) {
                if (sink.getId().equals(sinkId)) return entry.getKey();
            }
        }

        assert false;
        return null;
    }

    native void nativeOnSinksReceived(
            long nativeMediaRouterAndroid, String sourceUrn, int count);
    native void nativeOnRouteCreated(
            long nativeMediaRouterAndroid,
            String mediaRouteId,
            int createRouteRequestId,
            boolean wasLaunched);
    native void nativeOnRouteCreationError(
            long nativeMediaRouterAndroid, String errorText, int createRouteRequestId);
    native void nativeOnRouteClosed(long nativeMediaRouterAndroid, String mediaRouteId);
    native void nativeOnMessageSentResult(
            long nativeMediaRouterAndroid, boolean success, int callbackId);
    native void nativeOnMessage(long nativeMediaRouterAndroid, String mediaRouteId, String message);
}