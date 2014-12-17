// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.support.v4.content.WakefulBroadcastReceiver;

import org.chromium.components.devtools_bridge.ui.ServiceUIFactory;

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

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();

        mServer = new DevToolsBridgeServer(this, socketName(), newUIFactory());
    }

    @Override
    public void onDestroy() {
        mServer.dispose();

        super.onDestroy();
    }

    @Override
    public final int onStartCommand(Intent intent, int flags, int startId) {
        Runnable intentHandlingTask = mServer.startSticky();
        mServer.onStartCommand(intent);
        ReceiverBase.completeWakefulIntent(intent);
        intentHandlingTask.run(); // Stops self if no other task started.
        return START_NOT_STICKY;
    }

    protected abstract String socketName();
    protected abstract ServiceUIFactory newUIFactory();
}
