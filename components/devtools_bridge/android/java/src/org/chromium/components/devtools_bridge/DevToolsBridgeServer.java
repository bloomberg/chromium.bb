// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;

import org.chromium.components.devtools_bridge.util.LooperExecutor;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Responsibility of DevToolsBridgeServer consists of handling commands and managing sessions.
 * It designed to live in DevToolsBridgeServiceBase but also may live separately (in tests).
 */
public class DevToolsBridgeServer implements SignalingReceiver {
    private final LooperExecutor mExecutor;
    private final SessionDependencyFactory mFactory = SessionDependencyFactory.newInstance();
    private final Map<String, ServerSession> mSessions = new HashMap<String, ServerSession>();
    private final GCDNotificationHandler mHandler;
    private final Delegate mDelegate;

    /**
     * Callback for finding DevTools socket asynchronously. Needed in multiprocess
     * scenario when socket name is variable. May be called synchronously.
     */
    public interface QuerySocketCallback {
        void onSuccess(String socketName);
        void onFailure();
    }

    /**
     * Delegate abstracts Server from service lifetime management and UI.
     */
    public interface Delegate {
        Context getContext();

        // When runs in a service this service should not die when |sessionCount| > 0.
        void onSessionCountChange(int sessionCount);

        // Lets query a socket name when starting a new session. Result may change
        // (in multiprocess scenario: when browser process stops and then starts again).
        void querySocketName(QuerySocketCallback callback);
    }

    public DevToolsBridgeServer(Delegate delegate) {
        assert delegate != null;

        mExecutor = LooperExecutor.newInstanceForMainLooper(delegate.getContext());
        mHandler = new GCDNotificationHandler(this);
        mDelegate = delegate;
    }

    private void checkCalledOnHostServiceThread() {
        assert mExecutor.isCalledOnSessionThread();
    }

    public Context getContext() {
        return mDelegate.getContext();
    }

    public SharedPreferences getPreferences() {
        return getPreferences(getContext());
    }

    public static SharedPreferences getPreferences(Context context) {
        return context.getSharedPreferences(
                DevToolsBridgeServer.class.getName(), Context.MODE_PRIVATE);
    }

    public void handleCloudMessage(Intent cloudMessage, Runnable completionHandler) {
        if (mHandler.isNotification(cloudMessage)) {
            mHandler.onNotification(cloudMessage, completionHandler);
        } else {
            completionHandler.run();
        }
    }

    public void updateCloudMessagesId(String channelId, Runnable completionHandler) {
        mHandler.updateCloudMessagesId(channelId, completionHandler);
    }

    /**
     * Should be called in service's onDestroy.
     */
    public void dispose() {
        checkCalledOnHostServiceThread();

        for (ServerSession session : mSessions.values()) {
            session.dispose();
        }
        mFactory.dispose();
        mHandler.dispose();
    }

    @Override
    public void startSession(
            final String sessionId,
            final RTCConfiguration config,
            final String offer,
            final SessionBase.NegotiationCallback callback) {
        checkCalledOnHostServiceThread();
        if (mSessions.containsKey(sessionId)) {
            callback.onFailure("Session " + sessionId + " already exists");
            return;
        }

        mDelegate.querySocketName(new QuerySocketCallback() {
            @Override
            public void onSuccess(String socketName) {
                ServerSession session = new ServerSession(mFactory, mExecutor, socketName);
                session.setEventListener(new SessionEventListener(sessionId));
                mSessions.put(sessionId, session);
                session.startSession(config, offer, callback);
                mDelegate.onSessionCountChange(mSessions.size());
            }

            @Override
            public void onFailure() {
                callback.onFailure("Socket not available");
            }
        });
    }

    @Override
    public void renegotiate(
            String sessionId,
            String offer,
            SessionBase.NegotiationCallback callback) {
        checkCalledOnHostServiceThread();
        if (!mSessions.containsKey(sessionId)) {
            callback.onFailure("Session does not exist");
            return;
        }
        ServerSession session = mSessions.get(sessionId);
        session.renegotiate(offer, callback);
    }

    @Override
    public void iceExchange(
            String sessionId,
            List<String> clientCandidates,
            SessionBase.IceExchangeCallback callback) {
        checkCalledOnHostServiceThread();
        if (!mSessions.containsKey(sessionId)) {
            callback.onFailure("Session does not exist");
            return;
        }
        ServerSession session = mSessions.get(sessionId);
        session.iceExchange(clientCandidates, callback);
    }

    private class SessionEventListener implements SessionBase.EventListener {
        private final String mSessionId;

        public SessionEventListener(String sessionId) {
            mSessionId = sessionId;
        }

        public void onCloseSelf() {
            checkCalledOnHostServiceThread();

            mSessions.remove(mSessionId);
            mDelegate.onSessionCountChange(mSessions.size());
        }
    }

    public void closeAllSessions() {
        if (mSessions.isEmpty()) return;
        for (ServerSession session : mSessions.values()) {
            session.stop();
        }
        mSessions.clear();
        mDelegate.onSessionCountChange(mSessions.size());
    }
}
