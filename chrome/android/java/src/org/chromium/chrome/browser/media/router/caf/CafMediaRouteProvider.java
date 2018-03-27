// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf;

import org.chromium.chrome.browser.media.router.MediaController;
import org.chromium.chrome.browser.media.router.MediaRouteManager;
import org.chromium.chrome.browser.media.router.MediaRouteProvider;

import javax.annotation.Nullable;

/**
 * A {@link MediaRouteProvider} implementation for Cast devices and applications, using Cast v3 API.
 */
public class CafMediaRouteProvider implements MediaRouteProvider {
    MediaRouteManager mManager;

    private CafMediaRouteProvider(MediaRouteManager manager) {
        mManager = manager;
    }

    public static CafMediaRouteProvider create(MediaRouteManager manager) {
        return new CafMediaRouteProvider(manager);
    }

    @Override
    public boolean supportsSource(String sourceId) {
        // Not implemented.
        return true;
    }

    @Override
    public void startObservingMediaSinks(String sourceId) {
        // Not implemented.
    }

    @Override
    public void stopObservingMediaSinks(String sourceId) {
        // Not implemented.
    }

    @Override
    public void createRoute(String sourceId, String sinkId, String presentationId, String origin,
            int tabId, boolean isIncognito, int nativeRequestId) {
        // Not implemented.
    }

    @Override
    public void joinRoute(
            String sourceId, String presentationId, String origin, int tabId, int nativeRequestId) {
        // Not implemented.
    }

    @Override
    public void closeRoute(String routeId) {
        // Not implemented.
    }

    @Override
    public void detachRoute(String routeId) {
        // Not implemented.
    }

    @Override
    public void sendStringMessage(String routeId, String message, int nativeCallbackId) {
        // Not implemented.
    }

    @Override
    @Nullable
    public MediaController getMediaController(String routeId) {
        // Not implemented.
        return null;
    }
}
