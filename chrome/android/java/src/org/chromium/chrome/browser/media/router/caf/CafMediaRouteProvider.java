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
import org.chromium.chrome.browser.media.router.ChromeMediaRouter;
import org.chromium.chrome.browser.media.router.ClientRecord;
import org.chromium.chrome.browser.media.router.DiscoveryCallback;
import org.chromium.chrome.browser.media.router.DiscoveryDelegate;
import org.chromium.chrome.browser.media.router.FlingingController;
import org.chromium.chrome.browser.media.router.MediaRoute;
import org.chromium.chrome.browser.media.router.MediaRouteManager;
import org.chromium.chrome.browser.media.router.MediaRouteProvider;
import org.chromium.chrome.browser.media.router.MediaSink;
import org.chromium.chrome.browser.media.router.MediaSource;
import org.chromium.chrome.browser.media.router.cast.CastMediaSource;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * A {@link MediaRouteProvider} implementation for Cast devices and applications, using Cast v3 API.
 */
public class CafMediaRouteProvider
        implements MediaRouteProvider, DiscoveryDelegate, SessionManagerListener<CastSession> {
    private static final String TAG = "cr_CafMRP";

    private static final String AUTO_JOIN_PRESENTATION_ID = "auto-join";
    private static final String PRESENTATION_ID_SESSION_ID_PREFIX = "cast-session_";

    protected static final List<MediaSink> NO_SINKS = Collections.emptyList();

    protected final MediaRouter mAndroidMediaRouter;
    protected final MediaRouteManager mManager;
    protected final Map<String, DiscoveryCallback> mDiscoveryCallbacks =
            new HashMap<String, DiscoveryCallback>();
    protected Handler mHandler = new Handler();
    protected final Map<String, MediaRoute> mRoutes = new HashMap<String, MediaRoute>();
    private CreateRouteRequestInfo mPendingCreateRouteRequestInfo;
    private CastSessionController mSessionController;

    private ClientRecord mLastRemovedRouteRecord;
    private final Map<String, ClientRecord> mClientRecords = new HashMap<String, ClientRecord>();

    private CafMediaRouteProvider(MediaRouter androidMediaRouter, MediaRouteManager manager) {
        mAndroidMediaRouter = androidMediaRouter;
        mManager = manager;
    }

    public static CafMediaRouteProvider create(MediaRouteManager manager) {
        return new CafMediaRouteProvider(ChromeMediaRouter.getAndroidMediaRouter(), manager);
    }

    public Map<String, ClientRecord> getClientRecords() {
        return mClientRecords;
    }

    public Set<String> getClientIds() {
        return mClientRecords.keySet();
    }

    @Override
    public boolean supportsSource(String sourceId) {
        return getSourceFromId(sourceId) != null;
    }

    @Override
    public void startObservingMediaSinks(String sourceId) {
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

        MediaRouteSelector routeSelector = source.buildRouteSelector();
        if (routeSelector == null) {
            // If the application invalid, report no devices available.
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
    public void stopObservingMediaSinks(String sourceId) {
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

        MediaRouteSelector routeSelector = source.buildRouteSelector();
        if (routeSelector == null) {
            // If the application invalid, report no devices available.
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

    /**
     * Forward the sinks back to the native counterpart.
     */
    protected void onSinksReceivedInternal(String sourceId, @NonNull List<MediaSink> sinks) {
        Log.d(TAG, "Reporting %d sinks for source: %s", sinks.size(), sourceId);
        mManager.onSinksReceived(sourceId, this, sinks);
    }

    /**
     * {@link DiscoveryDelegate} implementation.
     */
    @Override
    public void onSinksReceived(String sourceId, @NonNull List<MediaSink> sinks) {
        Log.d(TAG, "Received %d sinks for sourceId: %s", sinks.size(), sourceId);
        mHandler.post(() -> { onSinksReceivedInternal(sourceId, sinks); });
    }

    @Override
    public void createRoute(String sourceId, String sinkId, String presentationId, String origin,
            int tabId, boolean isIncognito, int nativeRequestId) {
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

        MediaSource source = CastMediaSource.from(sourceId);
        if (source == null) {
            mManager.onRouteRequestError("Unsupported presentation URL", nativeRequestId);
            return;
        }

        mPendingCreateRouteRequestInfo = new CreateRouteRequestInfo(
                source, sink, presentationId, origin, tabId, isIncognito, nativeRequestId);

        CastUtils.getCastContext().setReceiverApplicationId(source.getApplicationId());

        for (MediaRouter.RouteInfo routeInfo : mAndroidMediaRouter.getRoutes()) {
            if (routeInfo.getId().equals(sink.getId())) {
                // Unselect and then select so that CAF will get notified of the selection.
                mAndroidMediaRouter.unselect(0);
                routeInfo.select();
                break;
            }
        }
    }

    @Override
    public void joinRoute(
            String sourceId, String presentationId, String origin, int tabId, int nativeRequestId) {
        CastMediaSource source = CastMediaSource.from(sourceId);
        if (source == null || source.getClientId() == null) {
            mManager.onRouteRequestError("Unsupported presentation URL", nativeRequestId);
            return;
        }

        if (!hasSession()) {
            mManager.onRouteRequestError("No presentation", nativeRequestId);
            return;
        }

        if (!canJoinExistingSession(presentationId, origin, tabId, source)) {
            mManager.onRouteRequestError("No matching route", nativeRequestId);
            return;
        }

        MediaRoute route =
                new MediaRoute(mSessionController.getSink().getId(), sourceId, presentationId);
        addRoute(route, origin, tabId);
        mManager.onRouteCreated(route.id, route.sinkId, nativeRequestId, this, false);
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

        ClientRecord client = getClientRecordByRouteId(routeId);
        if (client != null && mAndroidMediaRouter != null) {
            MediaSink sink =
                    MediaSink.fromSinkId(mSessionController.getSink().getId(), mAndroidMediaRouter);
            if (sink != null) {
                mSessionController.notifyReceiverAction(routeId, sink, client.clientId, "stop");
            }
        }

        mSessionController.endSession();
    }

    @Override
    public void detachRoute(String routeId) {
        mRoutes.remove(routeId);

        removeClient(getClientRecordByRouteId(routeId));
    }

    @Override
    public void sendStringMessage(String routeId, String message, int nativeCallbackId) {
        // Not implemented.
    }

    @Override
    @Nullable
    public FlingingController getFlingingController(String routeId) {
        // Not implemented.
        return null;
    }

    private MediaSource getSourceFromId(String sourceId) {
        return CastMediaSource.from(sourceId);
    }

    ///////////////////////////////////////////////
    // SessionManagerListener implementation
    ///////////////////////////////////////////////

    @Override
    public void onSessionStarting(CastSession session) {
        Log.d(TAG, "onSessionStarting");
        mSessionController = new CastSessionController(session, this,
                mPendingCreateRouteRequestInfo.sink, mPendingCreateRouteRequestInfo.source);
        MediaRoute route = new MediaRoute(mPendingCreateRouteRequestInfo.sink.getId(),
                mPendingCreateRouteRequestInfo.source.getSourceId(),
                mPendingCreateRouteRequestInfo.presentationId);
        addRoute(
                route, mPendingCreateRouteRequestInfo.origin, mPendingCreateRouteRequestInfo.tabId);
        mManager.onRouteCreated(
                route.id, route.sinkId, mPendingCreateRouteRequestInfo.nativeRequestId, this, true);

        String clientId = ((CastMediaSource) mPendingCreateRouteRequestInfo.source).getClientId();

        if (clientId != null) {
            ClientRecord clientRecord = mClientRecords.get(clientId);
            if (clientRecord != null) {
                mSessionController.notifyReceiverAction(clientRecord.routeId,
                        mPendingCreateRouteRequestInfo.sink, clientId, "cast");
            }
        }
    }

    @Override
    public void onSessionStarted(CastSession session, String sessionId) {
        Log.d(TAG, "onSessionStarted");
        mPendingCreateRouteRequestInfo = null;
        mSessionController.onSessionStarted();
    }

    @Override
    public void onSessionStartFailed(CastSession session, int error) {
        for (String routeId : mRoutes.keySet()) {
            mManager.onRouteClosedWithError(routeId, "Launch error");
        }
        mRoutes.clear();
        mClientRecords.clear();
    }

    @Override
    public void onSessionEnding(CastSession session) {
        // Not implemented.
    }

    @Override
    public void onSessionEnded(CastSession session, int error) {
        if (!hasSession()) return;

        if (mClientRecords.isEmpty()) {
            for (String routeId : mRoutes.keySet()) mManager.onRouteClosed(routeId);
            mRoutes.clear();
        } else {
            mLastRemovedRouteRecord = mClientRecords.values().iterator().next();
            for (ClientRecord client : mClientRecords.values()) {
                mManager.onRouteClosed(client.routeId);

                mRoutes.remove(client.routeId);
            }
            mClientRecords.clear();
        }

        detachFromSession();
        if (mAndroidMediaRouter != null) {
            mAndroidMediaRouter.selectRoute(mAndroidMediaRouter.getDefaultRoute());
        }
    }

    @Override
    public void onSessionResuming(CastSession session, String sessionId) {}

    @Override
    public void onSessionResumed(CastSession session, boolean wasSuspended) {}

    @Override
    public void onSessionResumeFailed(CastSession session, int error) {}

    @Override
    public void onSessionSuspended(CastSession session, int reason) {}

    public boolean hasSession() {
        return mSessionController != null;
    }

    private void detachFromSession() {
        mSessionController.onSessionEnded();
        mSessionController = null;
    }

    private void addRoute(MediaRoute route, String origin, int tabId) {
        mRoutes.put(route.id, route);

        CastMediaSource source = CastMediaSource.from(route.sourceId);
        final String clientId = source.getClientId();

        if (clientId == null || mClientRecords.containsKey(clientId)) return;

        mClientRecords.put(clientId,
                new ClientRecord(route.id, clientId, source.getApplicationId(),
                        source.getAutoJoinPolicy(), origin, tabId));
    }

    private void removeClient(@Nullable ClientRecord client) {
        if (client == null) return;

        mLastRemovedRouteRecord = client;
        mClientRecords.remove(client.clientId);
    }

    @Nullable
    private ClientRecord getClientRecordByRouteId(String routeId) {
        for (ClientRecord record : mClientRecords.values()) {
            if (record.routeId.equals(routeId)) return record;
        }
        return null;
    }

    private static class CreateRouteRequestInfo {
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

    private boolean canJoinExistingSession(
            String presentationId, String origin, int tabId, CastMediaSource source) {
        if (AUTO_JOIN_PRESENTATION_ID.equals(presentationId)) {
            return canAutoJoin(source, origin, tabId);
        }
        if (presentationId.startsWith(PRESENTATION_ID_SESSION_ID_PREFIX)) {
            String sessionId = presentationId.substring(PRESENTATION_ID_SESSION_ID_PREFIX.length());
            return mSessionController.getSession().getSessionId().equals(sessionId);
        }
        for (MediaRoute route : mRoutes.values()) {
            if (route.presentationId.equals(presentationId)) return true;
        }
        return false;
    }

    private boolean canAutoJoin(CastMediaSource source, String origin, int tabId) {
        if (source.getAutoJoinPolicy().equals(CastMediaSource.AUTOJOIN_PAGE_SCOPED)) return false;

        CastMediaSource currentSource = (CastMediaSource) mSessionController.getSource();
        if (!currentSource.getApplicationId().equals(source.getApplicationId())) return false;

        if (mClientRecords.isEmpty() && mLastRemovedRouteRecord != null) {
            return isSameOrigin(origin, mLastRemovedRouteRecord.origin)
                    && tabId == mLastRemovedRouteRecord.tabId;
        }

        if (mClientRecords.isEmpty()) return false;

        ClientRecord client = mClientRecords.values().iterator().next();

        if (source.getAutoJoinPolicy().equals(CastMediaSource.AUTOJOIN_ORIGIN_SCOPED)) {
            return isSameOrigin(origin, client.origin);
        }
        if (source.getAutoJoinPolicy().equals(CastMediaSource.AUTOJOIN_TAB_AND_ORIGIN_SCOPED)) {
            return isSameOrigin(origin, client.origin) && tabId == client.tabId;
        }

        return false;
    }

    /**
     * Compares two origins. Empty origin strings correspond to unique origins in
     * url::Origin.
     *
     * @param originA A URL origin.
     * @param originB A URL origin.
     * @return True if originA and originB represent the same origin, false otherwise.
     */
    private static final boolean isSameOrigin(String originA, String originB) {
        if (originA == null || originA.isEmpty() || originB == null || originB.isEmpty())
            return false;
        return originA.equals(originB);
    }
}
