// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.media.router.cast.remoting;

import android.support.v7.media.MediaRouter;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.media.router.ChromeMediaRouter;
import org.chromium.chrome.browser.media.router.MediaRoute;
import org.chromium.chrome.browser.media.router.MediaRouteManager;
import org.chromium.chrome.browser.media.router.MediaRouteProvider;
import org.chromium.chrome.browser.media.router.cast.BaseMediaRouteProvider;
import org.chromium.chrome.browser.media.router.cast.ChromeCastSessionManager;
import org.chromium.chrome.browser.media.router.cast.CreateRouteRequest;
import org.chromium.chrome.browser.media.router.cast.MediaSink;
import org.chromium.chrome.browser.media.router.cast.MediaSource;

/**
 * A {@link MediaRouteProvider} implementation for media remote playback.
 */
public class RemotingMediaRouteProvider extends BaseMediaRouteProvider {
    private static final String TAG = "MediaRemoting";

    /**
     * @return Initialized {@link RemotingMediaRouteProvider} object.
     */
    public static RemotingMediaRouteProvider create(MediaRouteManager manager) {
        return new RemotingMediaRouteProvider(ChromeMediaRouter.getAndroidMediaRouter(), manager);
    }

    @Override
    protected MediaSource getSourceFromId(String sourceId) {
        return RemotingMediaSource.from(sourceId);
    }

    @Override
    protected ChromeCastSessionManager.CastSessionLaunchRequest createSessionLaunchRequest(
            MediaSource source, MediaSink sink, String presentationId, String origin, int tabId,
            boolean isIncognito, int nativeRequestId) {
        return new CreateRouteRequest(source, sink, presentationId, origin, tabId, isIncognito,
                nativeRequestId, this, CreateRouteRequest.RequestedCastSessionType.REMOTE, null);
    }

    @Override
    public void joinRoute(
            String sourceId, String presentationId, String origin, int tabId, int nativeRequestId) {
        mManager.onRouteRequestError(
                "Remote playback doesn't support joining routes", nativeRequestId);
    }

    @Override
    public void closeRoute(String routeId) {
        MediaRoute route = mRoutes.get(routeId);
        if (route == null) return;

        if (mSession == null) {
            mRoutes.remove(routeId);
            mManager.onRouteClosed(routeId);
            return;
        }

        ChromeCastSessionManager.get().stopApplication();
    }

    @Override
    public void detachRoute(String routeId) {
        mRoutes.remove(routeId);
    }

    @Override
    public void sendStringMessage(String routeId, String message, int nativeCallbackId) {
        Log.e(TAG, "Remote playback does not support sending messages");
        mManager.onMessageSentResult(false, nativeCallbackId);
    }

    @VisibleForTesting
    RemotingMediaRouteProvider(MediaRouter androidMediaRouter, MediaRouteManager manager) {
        super(androidMediaRouter, manager);
    }

    @Override
    public void onSessionEnded() {
        if (mSession == null) return;

        for (String routeId : mRoutes.keySet()) mManager.onRouteClosed(routeId);
        mRoutes.clear();

        mSession = null;

        if (mAndroidMediaRouter != null) {
            mAndroidMediaRouter.selectRoute(mAndroidMediaRouter.getDefaultRoute());
        }
    }

    @Override
    public void onSessionStarting(ChromeCastSessionManager.CastSessionLaunchRequest launchRequest) {
        CreateRouteRequest request = (CreateRouteRequest) launchRequest;
        MediaSink sink = request.getSink();
        MediaSource source = request.getSource();

        MediaRoute route =
                new MediaRoute(sink.getId(), source.getSourceId(), request.getPresentationId());
        mRoutes.put(route.id, route);
        mManager.onRouteCreated(route.id, route.sinkId, request.getNativeRequestId(), this, true);
    }
}
