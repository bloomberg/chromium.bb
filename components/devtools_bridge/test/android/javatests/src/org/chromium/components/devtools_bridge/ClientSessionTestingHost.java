// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import java.io.IOException;
import java.util.List;

/**
 * Helper class which handles a client session in tests. Having direct reference to
 * the server it runs its client session on a dedicated thread and proxies all call
 * between them to satisfy theading requirements.
 */
public class ClientSessionTestingHost {
    private final SignalingReceiver mTarget;
    private final SessionBase.Executor mTargetExecutor;
    private final LocalSessionBridge.ThreadedExecutor mClientExecutor;
    private final String mSessionId;
    private int mDelayMs = 10;
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
        mClientSession = new ClientSession(
                factory,
                mClientExecutor,
                new TargetAdaptor().createProxy(),
                clientSocketName);
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
        mClientExecutor.runSynchronously(new Runnable() {
            @Override
            public void run() {
                mClientSession.start(new RTCConfiguration());
            }
        });
    }

    /**
     * Adapts ServerSessionInterface to DevToolsBridgeServer. Lives on mServerExecutor.
     */
    private class TargetAdaptor implements SessionBase.ServerSessionInterface {
        /**
         * Creates proxy that to safely use on mClientExecutor.
         */
        public LocalSessionBridge.ServerSessionProxy createProxy() {
            LocalSessionBridge.ServerSessionProxy proxy =
                    new LocalSessionBridge.ServerSessionProxy(
                            mTargetExecutor, mClientExecutor, this, mDelayMs);
            assert proxy.clientExecutor() == mClientExecutor;
            assert proxy.serverExecutor() == mTargetExecutor;
            return proxy;
        }

        @Override
        public void startSession(RTCConfiguration config,
                                 String offer,
                                 SessionBase.NegotiationCallback callback) {
            mTarget.startSession(mSessionId, config, offer, callback);
        }

        @Override
        public void renegotiate(String offer, SessionBase.NegotiationCallback callback) {
            mTarget.renegotiate(mSessionId, offer, callback);
        }

        @Override
        public void iceExchange(List<String> clientCandidates,
                                SessionBase.IceExchangeCallback callback) {
            mTarget.iceExchange(mSessionId, clientCandidates, callback);
        }
    }
}
