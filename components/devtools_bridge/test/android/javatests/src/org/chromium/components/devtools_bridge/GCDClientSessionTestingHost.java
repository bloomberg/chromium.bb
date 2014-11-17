// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.util.Log;

import org.chromium.components.devtools_bridge.apiary.TestApiaryClientFactory;
import org.chromium.components.devtools_bridge.commands.Command;
import org.chromium.components.devtools_bridge.commands.CommandSender;

import java.io.IOException;
import java.util.Random;

/**
 * Helper class which handles a client session in manual tests. Connects to a
 * remote server with GCD.
 */
public class GCDClientSessionTestingHost {
    private static final String TAG = "GCDClientSessionTestingHost";
    private static final Random sRandom = new Random();

    private final TestApiaryClientFactory mClientFactory;
    private final SessionDependencyFactory mFactory;
    private final LocalSessionBridge.ThreadedExecutor mIOExecutor;
    private final ClientSessionTestingHost mBase;
    private final String mOAuthToken;
    private final String mRemoteInstanceId;

    private volatile boolean mStarted;

    public GCDClientSessionTestingHost(
            String oAuthToken, String socketName, String remoteInstanceId) throws IOException {
        mClientFactory = new TestApiaryClientFactory();
        mFactory = SessionDependencyFactory.newInstance();
        mIOExecutor = new LocalSessionBridge.ThreadedExecutor();

        String sessionId = Integer.toString(sRandom.nextInt());

        mBase = new ClientSessionTestingHost(
                mFactory, new ServerProxy(remoteInstanceId), mIOExecutor, sessionId, socketName);

        mOAuthToken = oAuthToken;
        mRemoteInstanceId = remoteInstanceId;
    }

    public boolean isStarted() {
        return mStarted;
    }

    public void start(final Runnable completionCallback) {
        mIOExecutor.postOnSessionThread(0, new Runnable() {
                    @Override
                    public void run() {
                        try {
                            mBase.start(mClientFactory.newConfigClient().fetch());
                            mStarted = true;
                        } catch (IOException e) {
                            Log.e(TAG, "Failed to start", e);
                        } finally {
                            completionCallback.run();
                        }
                    }

                });
    }

    public void dispose() {
        mBase.dispose();
        mIOExecutor.runSynchronously(new Runnable() {
            @Override
            public void run() {
                // All previously scheduled operations have completed.
                mClientFactory.close();
            }
        });
        mIOExecutor.dispose();
    }

    private class ServerProxy extends CommandSender {
        private final String mInstanceId;

        public ServerProxy(String instanceId) {
            mInstanceId = instanceId;
        }

        protected void send(Command command, Runnable completionCallback) {
            assert mIOExecutor.isCalledOnSessionThread();

            try {
                mClientFactory.newTestGCDClient(mOAuthToken).send(mRemoteInstanceId, command);
            } catch (IOException e) {
                Log.e(TAG, "Exception when sending a command", e);
                command.setFailure("IOException: " + e.getMessage());
            } finally {
                completionCallback.run();
            }
        }
    }
}
