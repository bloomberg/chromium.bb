// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.media.router.cast.remoting;

import android.support.v7.media.MediaRouter;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.media.router.BaseMediaRouteProvider;
import org.chromium.chrome.browser.media.router.ChromeMediaRouter;
import org.chromium.chrome.browser.media.router.MediaRouteManager;
import org.chromium.chrome.browser.media.router.MediaRouteProvider;
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

    // TODO(avayvod): implement the methods below. See https://crbug.com/517102.
    @Override
    public void createRoute(String sourceId, String sinkId, String presentationId, String origin,
            int tabId, boolean isIncognito, int nativeRequestId) {}

    @Override
    public void joinRoute(String sourceId, String presentationId, String origin, int tabId,
            int nativeRequestId) {}

    @Override
    public void closeRoute(String routeId) {}

    @Override
    public void detachRoute(String routeId) {}

    @Override
    public void sendStringMessage(String routeId, String message, int nativeCallbackId) {}

    @VisibleForTesting
    RemotingMediaRouteProvider(MediaRouter androidMediaRouter, MediaRouteManager manager) {
        super(androidMediaRouter, manager);
    }
}
