// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.util.Log;

import java.io.IOException;

/**
 * Helper class which handles a client session in tests. Having direct reference to
 * the server it runs its client session on a dedicated thread and proxies all call
 * between them to satisfy theading requirements.
 */
public class ClientSessionTestingHost {
    private static final String TAG = "ClientSessionTestingHost";

    private final SignalingReceiver mTarget;
    private final SessionBase.Executor mTargetExecutor;
    private final LocalSessionBridge.ThreadedExecutor mClientExecutor;
    private final String mSessionId;
    private final ClientSession mClientSession;

    public ClientSessionTestingHost(
            SessionDependencyFactory factory,
            SignalingReceiver target, SessionBase.Executor targetExecutor,
            String sessionId, String clientSocketName)
            throws IOException {
        mTarget = target;
        mTargetExecutor = targetExecutor;
        mClientExecutor = new LocalSessionBridge.ThreadedExecutor();

        mSessionId = sessionId;

        SignalingReceiverProxy proxy = new SignalingReceiverProxy(
                mTargetExecutor, mClientExecutor, target, 0);

        mClientSession = new ClientSession(
                factory,
                mClientExecutor,
                proxy.asServerSession(mSessionId),
                clientSocketName) {
            @Override
            protected void closeSelf() {
                Log.d(TAG, "Closed self");
                super.closeSelf();
            }

            @Override
            protected void onControlChannelOpened() {
                Log.d(TAG, "Control channel opened");
                super.onControlChannelOpened();
            }

            @Override
            protected void onControlChannelClosed() {
                Log.d(TAG, "Control channel closed");
                super.onControlChannelClosed();
            }

            @Override
            protected void onFailure(String message) {
                Log.e(TAG, "Failure: " + message);
                super.onFailure(message);
            }
        };
    }

    public void dispose() {
        mClientExecutor.runSynchronously(new Runnable() {
            @Override
            public void run() {
                mClientSession.dispose();
            }
        });
    }

    public void start() {
        start(new RTCConfiguration());
    }

    public void start(final RTCConfiguration config) {
        mClientExecutor.runSynchronously(new Runnable() {
            @Override
            public void run() {
                mClientSession.start(config);
            }
        });
    }
}
