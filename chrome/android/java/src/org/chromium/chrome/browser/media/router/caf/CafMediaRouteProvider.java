// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf;

import android.support.annotation.Nullable;
import android.support.v7.media.MediaRouter;

import com.google.android.gms.cast.framework.CastSession;

import org.chromium.base.Log;
import org.chromium.chrome.browser.media.router.ChromeMediaRouter;
import org.chromium.chrome.browser.media.router.ClientRecord;
import org.chromium.chrome.browser.media.router.FlingingController;
import org.chromium.chrome.browser.media.router.MediaRoute;
import org.chromium.chrome.browser.media.router.MediaRouteManager;
import org.chromium.chrome.browser.media.router.MediaRouteProvider;
import org.chromium.chrome.browser.media.router.MediaSink;
import org.chromium.chrome.browser.media.router.MediaSource;
import org.chromium.chrome.browser.media.router.cast.CastMediaSource;

import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * A {@link MediaRouteProvider} implementation for Cast devices and applications, using Cast v3 API.
 */
public class CafMediaRouteProvider extends CafBaseMediaRouteProvider {
    private static final String TAG = "CafMRP";

    private static final String AUTO_JOIN_PRESENTATION_ID = "auto-join";
    private static final String PRESENTATION_ID_SESSION_ID_PREFIX = "cast-session_";

    private CreateRouteRequestInfo mPendingCreateRouteRequestInfo;

    private ClientRecord mLastRemovedRouteRecord;
    private final Map<String, ClientRecord> mClientRecords = new HashMap<String, ClientRecord>();
    private CafMessageHandler mMessageHandler;

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
    public void requestSessionLaunch(CreateRouteRequestInfo request) {
        CastUtils.getCastContext().setReceiverApplicationId(request.source.getApplicationId());

        for (MediaRouter.RouteInfo routeInfo : getAndroidMediaRouter().getRoutes()) {
            if (routeInfo.getId().equals(request.sink.getId())) {
                // Unselect and then select so that CAF will get notified of the selection.
                getAndroidMediaRouter().unselect(0);
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
                new MediaRoute(sessionController().getSink().getId(), sourceId, presentationId);
        addRoute(route, origin, tabId);
        mManager.onRouteCreated(route.id, route.sinkId, nativeRequestId, this, false);
    }

    // TODO(zqzhang): the clientRecord/route management is not clean and the logic seems to be
    // problematic.
    @Override
    public void closeRoute(String routeId) {
        boolean isRouteInRecord = mRoutes.containsKey(routeId);

        super.closeRoute(routeId);

        if (!isRouteInRecord) return;

        ClientRecord client = getClientRecordByRouteId(routeId);
        if (client != null) {
            MediaSink sink = MediaSink.fromSinkId(
                    sessionController().getSink().getId(), getAndroidMediaRouter());
            if (sink != null) {
                mMessageHandler.sendReceiverActionToClient(routeId, sink, client.clientId, "stop");
            }
        }
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

    @Override
    protected MediaSource getSourceFromId(String sourceId) {
        return CastMediaSource.from(sourceId);
    }

    public void sendMessageToClient(String clientId, String message) {
        ClientRecord clientRecord = mClientRecords.get(clientId);
        if (clientRecord == null) return;

        if (!clientRecord.isConnected) {
            Log.d(TAG, "Queueing message to client %s: %s", clientId, message);
            clientRecord.pendingMessages.add(message);
            return;
        }

        Log.d(TAG, "Sending message to client %s: %s", clientId, message);
        mManager.onMessage(clientRecord.routeId, message);
    }

    ///////////////////////////////////////////////
    // SessionManagerListener implementation
    ///////////////////////////////////////////////

    @Override
    public void onSessionStarting(CastSession session) {}

    @Override
    public void onSessionStarted(CreateRouteRequestInfo request) {
        Log.d(TAG, "onSessionStarted");

        MediaSink sink = request.sink;
        MediaSource source = request.source;

        MediaRoute route =
                new MediaRoute(sink.getId(), source.getSourceId(), request.presentationId);
        addRoute(route, request.origin, request.tabId);

        String clientId = ((CastMediaSource) source).getClientId();

        if (clientId != null) {
            ClientRecord clientRecord = mClientRecords.get(clientId);
            if (clientRecord != null) {
                mMessageHandler.sendReceiverActionToClient(
                        clientRecord.routeId, sink, clientId, "cast");
            }
        }

        mMessageHandler.onSessionStarted(sessionController());
        sessionController().getSession().getRemoteMediaClient().requestStatus();
    }

    @Override
    public void onSessionStartFailed(CastSession session, int error) {
        super.onSessionStartFailed(session, error);
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
        getAndroidMediaRouter().selectRoute(getAndroidMediaRouter().getDefaultRoute());
    }

    @Override
    public void onSessionResuming(CastSession session, String sessionId) {}

    @Override
    public void onSessionResumed(CastSession session, boolean wasSuspended) {}

    @Override
    public void onSessionResumeFailed(CastSession session, int error) {}

    @Override
    public void onSessionSuspended(CastSession session, int reason) {}

    private void addRoute(MediaRoute route, String origin, int tabId) {
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

    private CafMediaRouteProvider(MediaRouter androidMediaRouter, MediaRouteManager manager) {
        super(androidMediaRouter, manager);
    }

    private boolean canJoinExistingSession(
            String presentationId, String origin, int tabId, CastMediaSource source) {
        if (AUTO_JOIN_PRESENTATION_ID.equals(presentationId)) {
            return canAutoJoin(source, origin, tabId);
        }
        if (presentationId.startsWith(PRESENTATION_ID_SESSION_ID_PREFIX)) {
            String sessionId = presentationId.substring(PRESENTATION_ID_SESSION_ID_PREFIX.length());
            return sessionController().getSession().getSessionId().equals(sessionId);
        }
        for (MediaRoute route : mRoutes.values()) {
            if (route.presentationId.equals(presentationId)) return true;
        }
        return false;
    }

    private boolean canAutoJoin(CastMediaSource source, String origin, int tabId) {
        if (source.getAutoJoinPolicy().equals(CastMediaSource.AUTOJOIN_PAGE_SCOPED)) return false;

        CastMediaSource currentSource = (CastMediaSource) sessionController().getSource();
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

    Collection<String> getClients() {
        return mClientRecords.keySet();
    }

    Map<String, ClientRecord> getClientRecordss() {
        return mClientRecords;
    }
}
