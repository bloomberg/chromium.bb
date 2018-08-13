// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.media.router.caf;

import android.os.Handler;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.media.MediaRouteSelector;
import android.support.v7.media.MediaRouter;
import android.support.v7.media.MediaRouter.RouteInfo;

import com.google.android.gms.cast.framework.CastSession;
import com.google.android.gms.cast.framework.SessionManagerListener;

import org.chromium.base.Log;
import org.chromium.chrome.browser.media.router.DiscoveryCallback;
import org.chromium.chrome.browser.media.router.DiscoveryDelegate;
import org.chromium.chrome.browser.media.router.MediaRoute;
import org.chromium.chrome.browser.media.router.MediaRouteManager;
import org.chromium.chrome.browser.media.router.MediaRouteProvider;
import org.chromium.chrome.browser.media.router.MediaSink;
import org.chromium.chrome.browser.media.router.MediaSource;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A base provider containing common implementation for CAF-based {@link MediaRouteProvider}s.
 */
public abstract class CafBaseMediaRouteProvider
        implements MediaRouteProvider, DiscoveryDelegate, SessionManagerListener<CastSession> {
    private static final String TAG = "CafMR";

    protected static final List<MediaSink> NO_SINKS = Collections.emptyList();
    protected final MediaRouter mAndroidMediaRouter;
    protected final MediaRouteManager mManager;
    protected final Map<String, DiscoveryCallback> mDiscoveryCallbacks =
            new HashMap<String, DiscoveryCallback>();
    protected final Map<String, MediaRoute> mRoutes = new HashMap<String, MediaRoute>();
    protected Handler mHandler = new Handler();

    // There can be only one Cast session at the same time on Android.
    private CastSessionController mSessionController;
    private CreateRouteRequestInfo mPendingCreateRouteRequestInfo;

    protected CafBaseMediaRouteProvider(MediaRouter androidMediaRouter, MediaRouteManager manager) {
        mAndroidMediaRouter = androidMediaRouter;
        mManager = manager;
    }

    /**
     * @return A MediaSource object constructed from |sourceId|, or null if the derived class does
     * not support the source.
     */
    @Nullable
    protected abstract MediaSource getSourceFromId(@NonNull String sourceId);

    protected abstract void requestSessionLaunch(CreateRouteRequestInfo createRouteRequest);

    /**
     * Forward the sinks back to the native counterpart.
     */
    private final void onSinksReceivedInternal(String sourceId, @NonNull List<MediaSink> sinks) {
        Log.d(TAG, "Reporting %d sinks for source: %s", sinks.size(), sourceId);
        mManager.onSinksReceived(sourceId, this, sinks);
    }

    /**
     * {@link DiscoveryDelegate} implementation.
     */
    @Override
    public final void onSinksReceived(String sourceId, @NonNull List<MediaSink> sinks) {
        Log.d(TAG, "Received %d sinks for sourceId: %s", sinks.size(), sourceId);
        mHandler.post(() -> { onSinksReceivedInternal(sourceId, sinks); });
    }

    @Override
    public final boolean supportsSource(String sourceId) {
        return getSourceFromId(sourceId) != null;
    }

    @Override
    public final void startObservingMediaSinks(String sourceId) {
        Log.d(TAG, "startObservingMediaSinks: " + sourceId);

        if (mAndroidMediaRouter == null) {
            // If the MediaRouter API is not available, report no devices so the page doesn't even
            // try to cast.
            onSinksReceived(sourceId, NO_SINKS);
            return;
        }

        MediaSource source = getSourceFromId(sourceId);
        if (source == null) {
            // If the source is invalid or not supported by this provider, report no devices
            // available.
            onSinksReceived(sourceId, NO_SINKS);
            return;
        }

        // No-op, if already monitoring the application for this source.
        String applicationId = source.getApplicationId();
        DiscoveryCallback callback = mDiscoveryCallbacks.get(applicationId);
        if (callback != null) {
            callback.addSourceUrn(sourceId);
            return;
        }

        MediaRouteSelector routeSelector = source.buildRouteSelector();
        if (routeSelector == null) {
            // If the application invalid, report no devices available.
            onSinksReceived(sourceId, NO_SINKS);
            return;
        }

        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        for (RouteInfo route : mAndroidMediaRouter.getRoutes()) {
            if (route.matchesSelector(routeSelector)) {
                knownSinks.add(MediaSink.fromRoute(route));
            }
        }

        callback = new DiscoveryCallback(sourceId, knownSinks, this, routeSelector);
        mAndroidMediaRouter.addCallback(
                routeSelector, callback, MediaRouter.CALLBACK_FLAG_REQUEST_DISCOVERY);
        mDiscoveryCallbacks.put(applicationId, callback);
    }

    @Override
    public final void stopObservingMediaSinks(String sourceId) {
        Log.d(TAG, "startObservingMediaSinks: " + sourceId);

        if (mAndroidMediaRouter == null) return;

        MediaSource source = getSourceFromId(sourceId);
        if (source == null) return;

        // No-op, if already monitoring the application for this source.
        String applicationId = source.getApplicationId();
        DiscoveryCallback callback = mDiscoveryCallbacks.get(applicationId);
        if (callback == null) return;

        callback.removeSourceUrn(sourceId);

        if (callback.isEmpty()) {
            mAndroidMediaRouter.removeCallback(callback);
            mDiscoveryCallbacks.remove(applicationId);
        }
    }

    @Override
    public final void createRoute(String sourceId, String sinkId, String presentationId,
            String origin, int tabId, boolean isIncognito, int nativeRequestId) {
        Log.d(TAG, "createRoute");
        if (mPendingCreateRouteRequestInfo != null) {
            // TODO(zqzhang): do something.
        }
        if (mAndroidMediaRouter == null) {
            mManager.onRouteRequestError("Not supported", nativeRequestId);
            return;
        }

        MediaSink sink = MediaSink.fromSinkId(sinkId, mAndroidMediaRouter);
        if (sink == null) {
            mManager.onRouteRequestError("No sink", nativeRequestId);
            return;
        }

        MediaSource source = getSourceFromId(sourceId);
        if (source == null) {
            mManager.onRouteRequestError("Unsupported source URL", nativeRequestId);
            return;
        }

        mPendingCreateRouteRequestInfo = new CreateRouteRequestInfo(
                source, sink, presentationId, origin, tabId, isIncognito, nativeRequestId);

        requestSessionLaunch(mPendingCreateRouteRequestInfo);
    }

    @Override
    public void closeRoute(String routeId) {
        MediaRoute route = mRoutes.get(routeId);
        if (route == null) return;

        if (!hasSession()) {
            mRoutes.remove(routeId);
            mManager.onRouteClosed(routeId);
            return;
        }

        mSessionController.endSession();
    }

    @Override
    public void onSessionStartFailed(CastSession session, int error) {
        for (String routeId : mRoutes.keySet()) {
            mManager.onRouteClosedWithError(routeId, "Launch error");
        }
        mRoutes.clear();
    }

    @Override
    public final void onSessionStarted(CastSession session, String sessionId) {
        mSessionController = new CastSessionController(session, this,
                mPendingCreateRouteRequestInfo.sink, mPendingCreateRouteRequestInfo.source);

        onSessionStarted(mPendingCreateRouteRequestInfo);

        MediaSink sink = mPendingCreateRouteRequestInfo.sink;
        MediaSource source = mPendingCreateRouteRequestInfo.source;
        MediaRoute route = new MediaRoute(
                sink.getId(), source.getSourceId(), mPendingCreateRouteRequestInfo.presentationId);
        mRoutes.put(route.id, route);
        mManager.onRouteCreated(
                route.id, route.sinkId, mPendingCreateRouteRequestInfo.nativeRequestId, this, true);

        mPendingCreateRouteRequestInfo = null;
    }

    // TODO(zqzhang): this is a temporary workaround for give CafMRP to manage ClientRecords on
    // session start. This needs to be removed once ClientRecord management gets refactored.
    abstract void onSessionStarted(CreateRouteRequestInfo request);

    protected boolean hasSession() {
        return mSessionController != null;
    }

    protected CastSessionController sessionController() {
        return mSessionController;
    }

    // TODO(zqzhang): This should go away once the session controller becomes a sticky instance.
    protected void detachFromSession() {
        mSessionController = null;
    }

    protected static class CreateRouteRequestInfo {
        public final MediaSource source;
        public final MediaSink sink;
        public final String presentationId;
        public final String origin;
        public final int tabId;
        public final boolean isIncognito;
        public final int nativeRequestId;

        public CreateRouteRequestInfo(MediaSource source, MediaSink sink, String presentationId,
                String origin, int tabId, boolean isIncognito, int nativeRequestId) {
            this.source = source;
            this.sink = sink;
            this.presentationId = presentationId;
            this.origin = origin;
            this.tabId = tabId;
            this.isIncognito = isIncognito;
            this.nativeRequestId = nativeRequestId;
        }
    }
}
