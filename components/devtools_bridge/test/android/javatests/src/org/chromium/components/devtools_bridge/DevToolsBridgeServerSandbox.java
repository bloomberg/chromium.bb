// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.content.Context;
import android.os.Process;
import android.util.Log;

import org.chromium.components.devtools_bridge.util.LooperExecutor;

import java.io.IOException;

/**
 * Sandbox for manual testing {@link DevToolsBridgeServer}
 */
public class DevToolsBridgeServerSandbox {
    private static final String TAG = "DevToolsBridgeServerSandbox";
    private static final String SERVER_SOCKET_NAME = "chrome_shell_devtools_remote";

    private Context mContext;
    private LooperExecutor mExecutor;
    private DevToolsBridgeServer mServer;
    private SessionDependencyFactory mFactory;
    private ClientSessionTestingHost mClientSession1;
    private ClientSessionTestingHost mClientSession2;

    public void start(Context context) {
        assert mContext == null;
        mContext = context;
        mExecutor = LooperExecutor.newInstanceForMainLooper(mContext);
        mServer = new DevToolsBridgeServer(new ServerDelegate(context));

        mFactory = SessionDependencyFactory.newInstance();
        mClientSession1 = createSession("Session 1", "webview_devtools_remote_" + Process.myPid());
        mClientSession2 = createSession("Session 2", "chrome_devtools_remote_" + Process.myPid());

        mClientSession1.start();
        mClientSession2.start();
    }

    public void stop() {
        mClientSession1.dispose();
        mClientSession2.dispose();
        mFactory.dispose();
        mClientSession1 = null;
        mClientSession2 = null;
        mFactory = null;
        mServer.dispose();
        mServer = null;
        mContext = null;
    }

    private ClientSessionTestingHost createSession(String sessionId, String socketName) {
        try {
            return new ClientSessionTestingHost(
                    mFactory, mServer, mExecutor, sessionId, socketName);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
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

    private static class ServerDelegate implements DevToolsBridgeServer.Delegate {
        private Context mContext;

        public ServerDelegate(Context context) {
            mContext = context;
        }

        @Override
        public Context getContext() {
            return mContext;
        }

        @Override
        public void onSessionCountChange(int sessionCount) {
            Log.i(TAG, sessionCount + " active session");
        }

        @Override
        public void querySocketName(DevToolsBridgeServer.QuerySocketCallback callback) {
            callback.onSuccess(SERVER_SOCKET_NAME);
        }
    }
}
