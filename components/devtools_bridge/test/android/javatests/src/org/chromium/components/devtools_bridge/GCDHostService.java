// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.content.Context;
import android.content.Intent;

import com.google.android.gcm.GCMBaseIntentService;
import com.google.android.gcm.GCMBroadcastReceiver;

import org.chromium.components.devtools_bridge.apiary.TestApiaryClientFactory;
import org.chromium.components.devtools_bridge.ui.ServiceUIFactory;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Service for manual testing DevToolsBridgeServer with GCD signaling.
 *
 * Since GCMBaseIntentService is IntentService it creates own HandlerThread
 * in onCreate and destroys it in onDestroy. It calls on onHandleIntent on this thread
 * and stopSelf(int) when finished. This behavior doesn't let to keep connections.
 * To change that behaviour this class doesn't pass onStartCommand to the IntentService.
 * Instead it calls onHandleIntent on its executor. GCMBaseIntentService do its
 * job on that executor and calls virtual methods (onMessage, onError, etc) on it.
 */
public final class GCDHostService extends GCMBaseIntentService {
    private static final String SOCKET_NAME = "chrome_shell_devtools_remote";

    private TestApiaryClientFactory mClientFactory;
    private DevToolsBridgeServer mServer;
    private GCDNotificationHandler mHandler;
    private ExecutorService mExecutor;

    // Accessed on mExecutor.
    private Runnable mCurrentCompletionHandler;

    /**
     * Cloud Messages broadcast receiver.
     */
    public static class Receiver extends GCMBroadcastReceiver {
        @Override
        protected String getGCMIntentServiceClassName(Context context) {
            return GCDHostService.class.getName();
        }
    }

    @Override
    protected String[] getSenderIds(Context context) {
        return new String[] { mClientFactory.getGCMSenderId() };
    }

    @Override
    public void onCreate() {
        mClientFactory = new TestApiaryClientFactory();
        mServer = new DevToolsBridgeServer(this, SOCKET_NAME, new UIFactoryImpl());
        mHandler = new GCDNotificationHandler(mServer, mClientFactory);
        mExecutor = Executors.newSingleThreadExecutor();
    }

    @Override
    public void onDestroy() {
        mServer.dispose();
        mHandler.dispose();
        mExecutor.submit(new Runnable() {
            @Override
            public void run() {
                mClientFactory.close();
            }
        });
        mExecutor.shutdown();
    }

    @Override
    public int onStartCommand(final Intent intent, int flags, int startId) {
        final Runnable completionHandler = mServer.startSticky();
        mExecutor.submit(new Runnable() {
            @Override
            public void run() {
                mCurrentCompletionHandler = completionHandler;
                onHandleIntent(intent);
                if (mCurrentCompletionHandler != null) {
                    mCurrentCompletionHandler.run();
                    mCurrentCompletionHandler = null;
                }
            }
        });
        return START_NOT_STICKY;
    }

    @Override
    protected void onError(Context context, String errorId) {
    }

    @Override
    protected void onMessage(Context context, final Intent intent) {
        if (!mHandler.isNotification(intent)) return;

        assert mCurrentCompletionHandler != null;
        final Runnable completionHandler = mCurrentCompletionHandler;
        mCurrentCompletionHandler = null; // Prevents completion in onStartCommand.

        mServer.postOnServiceThread(new Runnable() {
            @Override
            public void run() {
                mHandler.onNotification(intent, completionHandler);
            }
        });
    }

    @Override
    protected void onRegistered(Context context, String registrationId) {
    }

    @Override
    protected void onUnregistered(Context context, String registrationId) {
    }

    private static class UIFactoryImpl extends ServiceUIFactory {
        protected String productName() {
            return "GCDHostService";
        }

        protected int notificationSmallIcon() {
            return android.R.drawable.alert_dark_frame;
        }
    }
}
