// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import org.chromium.components.devtools_bridge.commands.Command;
import org.chromium.components.devtools_bridge.commands.CommandReceiver;
import org.chromium.components.devtools_bridge.commands.CommandSender;

import java.util.List;

/**
 * Helper proxy that binds client and server sessions living on different executors.
 */
final class SignalingReceiverProxy extends CommandSender {
    private final CommandReceiver mReceiver;
    private final SessionBase.Executor mServerExecutor;
    private final SessionBase.Executor mClientExecutor;
    private final int mDelayMs;

    public SignalingReceiverProxy(
            SessionBase.Executor serverExecutor,
            SessionBase.Executor clientExecutor,
            SignalingReceiver server,
            int delayMs) {
        mServerExecutor = serverExecutor;
        mClientExecutor = clientExecutor;
        mReceiver = new CommandReceiver(server);
        mDelayMs = delayMs;
    }

    public SignalingReceiverProxy(
            SessionBase.Executor serverExecutor,
            SessionBase.Executor clientExecutor,
            SessionBase.ServerSessionInterface serverSession,
            String sessionId,
            int delayMs) {
        this(serverExecutor, clientExecutor,
                new SignalingReceiverAdaptor(serverSession, sessionId),
                delayMs);
    }

    public SessionBase.Executor serverExecutor() {
        return mServerExecutor;
    }

    public SessionBase.Executor clientExecutor() {
        return mClientExecutor;
    }

    @Override
    protected void send(final Command command, final Runnable completionCallback) {
        assert mClientExecutor.isCalledOnSessionThread();

        mServerExecutor.postOnSessionThread(mDelayMs, new Runnable() {
                    @Override
                    public void run() {
                        mReceiver.receive(command, new Runnable() {
                            @Override
                            public void run() {
                                assert mServerExecutor.isCalledOnSessionThread();
                                mClientExecutor.postOnSessionThread(mDelayMs, completionCallback);
                            }
                        });
                    }
                });
    }

    public SessionBase.ServerSessionInterface asServerSession(String sessionId) {
        return new ServerSessionAdapter(this, sessionId);
    }

    private static final class ServerSessionAdapter implements SessionBase.ServerSessionInterface {
        private final SignalingReceiver mAdaptee;
        private final String mSessionId;

        public ServerSessionAdapter(SignalingReceiver adaptee, String sessionId) {
            mAdaptee = adaptee;
            mSessionId = sessionId;
        }

        @Override
        public void startSession(
                RTCConfiguration config, String offer, SessionBase.NegotiationCallback callback) {
            mAdaptee.startSession(mSessionId, config, offer, callback);
        }

        @Override
        public void renegotiate(String offer, SessionBase.NegotiationCallback callback) {
            mAdaptee.renegotiate(mSessionId, offer, callback);
        }

        @Override
        public void iceExchange(
                List<String> clientCandidates, SessionBase.IceExchangeCallback callback) {
            mAdaptee.iceExchange(mSessionId, clientCandidates, callback);
        }
    }

    private static final class SignalingReceiverAdaptor implements SignalingReceiver {
        private final SessionBase.ServerSessionInterface mAdaptee;
        private final String mSessionId;

        public SignalingReceiverAdaptor(
                SessionBase.ServerSessionInterface adaptee, String sessionId) {
            mAdaptee = adaptee;
            mSessionId = sessionId;
        }

        @Override
        public void startSession(
                String sessionId, RTCConfiguration config, String offer,
                SessionBase.NegotiationCallback callback) {
            if (mSessionId.equals(sessionId)) {
                mAdaptee.startSession(config, offer, callback);
            }
        }

        @Override
        public void renegotiate(
                String sessionId, String offer, SessionBase.NegotiationCallback callback) {
            if (mSessionId.equals(sessionId)) {
                mAdaptee.renegotiate(offer, callback);
            }
        }

        @Override
        public void iceExchange(
                String sessionId, List<String> clientCandidates,
                SessionBase.IceExchangeCallback callback) {
            if (mSessionId.equals(sessionId)) {
                mAdaptee.iceExchange(clientCandidates, callback);
            }
        }
    }
}
