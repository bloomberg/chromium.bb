// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.Process;
import android.util.Log;
import android.widget.Toast;

import java.io.IOException;

/**
 * Sandbox for manual testing {@link DevToolsBridgeServer}
 */
public class DevToolsBridgeServerSandbox {
    private static final String TAG = "DevToolsBridgeServerSandbox";
    private Context mContext;

    private final ServiceConnection mServiceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mBinder = ((LocalBindingTestService.LocalBinder) service);
            startInternal();
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            stopInternal();
            mBinder = null;
        }
    };

    private LocalBindingTestService.LocalBinder mBinder;

    /**
     * Service for hosting DevToolsBridgeServer in manual tests which exposes
     * Chrome Shell DevTools socket.
     */
    public static class Service extends LocalBindingTestService {
        public Service() {
            super("chrome_shell_devtools_remote");
        }
    }

    public void start(Context context) {
        assert mContext == null;
        mContext = context;
        Intent intent = new Intent(mContext, Service.class);
        mContext.bindService(
                intent, mServiceConnection, Context.BIND_DEBUG_UNBIND | Context.BIND_AUTO_CREATE);
    }

    public void stop() throws SecurityException {
        mContext.unbindService(mServiceConnection);
        mContext = null;
    }

    private SessionDependencyFactory mFactory;
    private ClientSessionTestingHost mClientSession1;
    private ClientSessionTestingHost mClientSession2;

    private void startInternal() {
        Log.d(TAG, "Service connected. Starting client sessions");
        Toast.makeText(mContext, "Connected to the service", Toast.LENGTH_SHORT).show();

        createConnections();
        mClientSession1.start();
        mClientSession2.start();
    }

    private void createConnections() {
        assert mClientSession1 == null;
        assert mClientSession2 == null;
        mFactory = SessionDependencyFactory.newInstance();

        String pid = Integer.toString(Process.myPid());
        try {
            mClientSession1 = createSessionHost(
                    "Session 1", "webview_devtools_remote_" + pid);
            mClientSession2 = createSessionHost(
                    "Session 2", "chrome_devtools_remote_" + pid);
        } catch (IOException e) {
            e.printStackTrace();
            Log.d(TAG, "Failed to start client sessions");
        }
    }

    private ClientSessionTestingHost createSessionHost(String sessionId, String socketName)
            throws IOException {
        return new ClientSessionTestingHost(
                mFactory, mBinder.getReceiver(), mBinder.createExecutor(), sessionId, socketName);
    }

    private void stopInternal() {
        Log.d(TAG, "Service disconnected. Stopping client sessions");
        mClientSession1.dispose();
        mClientSession2.dispose();
        mFactory.dispose();
        mClientSession1 = null;
        mClientSession2 = null;
        mFactory = null;
    }
}
