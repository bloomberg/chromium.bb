// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.os.Looper;
import android.support.v4.content.WakefulBroadcastReceiver;

import com.google.ipc.invalidation.external.client.contrib.MultiplexingGcmListener;

/**
 * Base class for a service which hosts DevToolsBridgeServer. It relies on Cloud Messages
 * intents rebroadcasted via MultiplexingGcmListener. It intentionally doesn't inherit from
 * MultiplexingGcmListener.AbstractListener because this service may not be IntentService
 * due to lifetime management restrictions of it.
 *
 * Service lives while any intent being processed or any client connected.
 * TODO(serya): Service lifetime management code should be moved here from DevToolsBridgeServer.
 */
public abstract class DevToolsBridgeServiceBase extends Service {
    private static final String WAKELOCK_KEY = "DevToolsBridgeService.WAKELOCK";

    /**
     * Delivers intents from MultiplexingGcmListener to the service making sure
     * wakelock is kept during the process.
     */
    protected abstract static class ReceiverBase extends WakefulBroadcastReceiver {
        private final Class<? extends DevToolsBridgeServiceBase> mConcreteServiceClass;

        protected ReceiverBase(Class<? extends DevToolsBridgeServiceBase> concreteServiceClass) {
            mConcreteServiceClass = concreteServiceClass;
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            // Pass all intent extras to the service.
            Intent startIntent = new Intent(context, mConcreteServiceClass);
            startIntent.setAction(intent.getAction());
            if (intent.getExtras() != null) startIntent.putExtras(intent.getExtras());
            startWakefulService(context, startIntent);
        }
    }

    private DevToolsBridgeServer mServer;
    private ServiceLifetimeManager mLifetimeManager;

    private Runnable mSessionHandlingTask;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mServer = new DevToolsBridgeServer(new ServerDelegate());
        mLifetimeManager = new ServiceLifetimeManager(this, WAKELOCK_KEY);
    }

    @Override
    public void onDestroy() {
        mServer.dispose();

        super.onDestroy();
    }

    @Override
    public final int onStartCommand(Intent intent, int flags, int startId) {
        Runnable intentHandlingTask = mLifetimeManager.startTask(startId);
        if (MultiplexingGcmListener.Intents.ACTION.equals(intent.getAction())) {
            handleGCMIntent(intent);
        } else {
            onHandleIntent(intent);
        }
        ReceiverBase.completeWakefulIntent(intent);
        intentHandlingTask.run(); // Stops self if no other task started.
        return START_NOT_STICKY;
    }

    private void handleGCMIntent(Intent intent) {
        if (intent.getBooleanExtra(MultiplexingGcmListener.Intents.EXTRA_OP_MESSAGE, false)) {
            mServer.handleCloudMessage(intent, mLifetimeManager.startTask());
        } else if (intent.getBooleanExtra(
                MultiplexingGcmListener.Intents.EXTRA_OP_REGISTERED, false)) {
            mServer.updateCloudMessagesId(
                    intent.getStringExtra(MultiplexingGcmListener.Intents.EXTRA_DATA_REG_ID),
                    startTask());
        } else if (intent.getBooleanExtra(
                MultiplexingGcmListener.Intents.EXTRA_OP_UNREGISTERED, false)) {
            mServer.updateCloudMessagesId("", startTask());
        }
    }

    /**
     * Unlike similar method in IntentService this one runs on service thread.
     * Cloud Messages intents are handled separately.
     */
    protected void onHandleIntent(Intent intent) {
        assert calledOnServiceThread();
    }

    protected abstract void querySocketName(DevToolsBridgeServer.QuerySocketCallback callback);
    protected abstract void onFirstSessionStarted();
    protected abstract void onLastSessionStopped();
    protected void onSessionCountChange(int sessionCount) {}

    protected DevToolsBridgeServer server() {
        return mServer;
    }

    protected Runnable startTask() {
        return mLifetimeManager.startTask();
    }

    protected boolean calledOnServiceThread() {
        return Looper.myLooper() == getMainLooper();
    }

    private class ServerDelegate implements DevToolsBridgeServer.Delegate {
        @Override
        public Context getContext() {
            return DevToolsBridgeServiceBase.this;
        }

        @Override
        public void onSessionCountChange(int sessionCount) {
            assert calledOnServiceThread();
            if (sessionCount > 0 && mSessionHandlingTask == null) {
                mSessionHandlingTask = startTask();
                DevToolsBridgeServiceBase.this.onFirstSessionStarted();
            } else if (sessionCount == 0 && mSessionHandlingTask != null) {
                mSessionHandlingTask.run();
                mSessionHandlingTask = null;
                DevToolsBridgeServiceBase.this.onLastSessionStopped();
            } else {
                DevToolsBridgeServiceBase.this.onSessionCountChange(sessionCount);
            }
        }

        @Override
        public void querySocketName(DevToolsBridgeServer.QuerySocketCallback callback) {
            DevToolsBridgeServiceBase.this.querySocketName(callback);
        }
    }
}
