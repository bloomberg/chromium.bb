// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.PowerManager;

import org.chromium.components.devtools_bridge.ui.ServiceUIFactory;
import org.chromium.components.devtools_bridge.util.LooperExecutor;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Android service mixin implementing DevTools Bridge features that not depend on
 * WebRTC signaling. Ability to host this class in different service classes allows:
 * 1. Parametrization.
 * 2. Simplified signaling for tests.
 *
 * Service starts foreground once any remote client starts a debugging session. Stops when all
 * remote clients disconnect.
 *
 * Must be called on service's main thread.
 */
public class DevToolsBridgeServer implements SignalingReceiver {
    public final int NOTIFICATION_ID = 1;
    public final String DISCONNECT_ALL_CLIENTS_ACTION =
            "action.DISCONNECT_ALL_CLIENTS_ACTION";

    public final String WAKELOCK_KEY = "wake_lock.DevToolsBridgeServer";

    private final Service mHost;
    private final String mSocketName;
    private final ServiceUIFactory mServiceUIFactory;
    private final LooperExecutor mExecutor;
    private final SessionDependencyFactory mFactory = SessionDependencyFactory.newInstance();
    private final Map<String, ServerSession> mSessions = new HashMap<String, ServerSession>();
    private final GCDNotificationHandler mHandler;
    private PowerManager.WakeLock mWakeLock;
    private Runnable mForegroundCompletionCallback;

    public DevToolsBridgeServer(Service host, String socketName, ServiceUIFactory uiFactory) {
        mHost = host;
        mSocketName = socketName;
        mServiceUIFactory = uiFactory;
        mExecutor = LooperExecutor.newInstanceForMainLooper(mHost);
        mHandler = new GCDNotificationHandler(this);

        checkCalledOnHostServiceThread();
    }

    private void checkCalledOnHostServiceThread() {
        assert mExecutor.isCalledOnSessionThread();
    }

    public Service getContext() {
        return mHost;
    }

    public SharedPreferences getPreferences() {
        return getPreferences(mHost);
    }

    public static SharedPreferences getPreferences(Context context) {
        return context.getSharedPreferences(
                DevToolsBridgeServer.class.getName(), Context.MODE_PRIVATE);
    }

    public void onStartCommand(Intent intent) {
        String action = intent.getAction();
        if (DISCONNECT_ALL_CLIENTS_ACTION.equals(action)) {
            closeAllSessions();
        } else if (mHandler.isNotification(intent)) {
            mHandler.onNotification(intent, startSticky());
        }
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
            String sessionId,
            RTCConfiguration config,
            String offer,
            SessionBase.NegotiationCallback callback) {
        checkCalledOnHostServiceThread();
        if (mSessions.containsKey(sessionId)) {
            callback.onFailure("Session already exists");
            return;
        }

        ServerSession session = new ServerSession(mFactory, mExecutor, mSocketName);
        session.setEventListener(new SessionEventListener(sessionId));
        mSessions.put(sessionId, session);
        session.startSession(config, offer, callback);
        if (mSessions.size() == 1)
            startForeground();
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

    protected void startForeground() {
        mForegroundCompletionCallback = startSticky();
        checkCalledOnHostServiceThread();
        mHost.startForeground(
                NOTIFICATION_ID,
                mServiceUIFactory.newForegroundNotification(mHost, DISCONNECT_ALL_CLIENTS_ACTION));
    }

    protected void stopForeground() {
        checkCalledOnHostServiceThread();
        mHost.stopForeground(true);
        mForegroundCompletionCallback.run();
        mForegroundCompletionCallback = null;
    }

    public void postOnServiceThread(Runnable runnable) {
        mExecutor.postOnSessionThread(0, runnable);
    }

    private class SessionEventListener implements SessionBase.EventListener {
        private final String mSessionId;

        public SessionEventListener(String sessionId) {
            mSessionId = sessionId;
        }

        public void onCloseSelf() {
            checkCalledOnHostServiceThread();

            mSessions.remove(mSessionId);
            if (mSessions.size() == 0) {
                stopForeground();
            }
        }
    }

    private void closeAllSessions() {
        if (mSessions.isEmpty()) return;
        for (ServerSession session : mSessions.values()) {
            session.stop();
        }
        mSessions.clear();
        stopForeground();
    }

    /**
     * TODO(serya): Move service lifetime management to DevToolsBridgeServiceBase.
     * Helper method for doing background tasks. Usage:
     *
     * int onStartCommand(...) {
     *     if (..*) {
     *         startWorkInBackground(startSticky());
     *         return START_STICKY;
     *     }
     *     ...
     * }
     *
     * void doWorkInBackground(final Runable completionHandler) {
     *     ... start background task
     *         @Override
     *         void run() {
     *             ...
     *             completionHandler.run();
     *         }
     * }
     */
    public Runnable startSticky() {
        checkCalledOnHostServiceThread();
        if (mWakeLock == null) {
            PowerManager pm = (PowerManager) mHost.getSystemService(Context.POWER_SERVICE);
            mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, WAKELOCK_KEY);
        }
        mWakeLock.acquire();
        return new StartStickyCompletionHandler();
    }

    private class StartStickyCompletionHandler implements Runnable {
        @Override
        public void run() {
            postOnServiceThread(new Runnable() {
                @Override
                public void run() {
                    mWakeLock.release();
                    if (!mWakeLock.isHeld()) mHost.stopSelf();
                }
            });
        }
    }
}
