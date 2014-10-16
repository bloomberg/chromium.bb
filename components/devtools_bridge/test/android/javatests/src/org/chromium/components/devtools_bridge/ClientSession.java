// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import java.io.IOException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Client session. Creates client socket tunnel for clientSocketName as a default tunnel.
 * See SessionBase for details.
 */
public class ClientSession extends SessionBase {
    private final ServerSessionInterface mServer;
    private RTCConfiguration mConfig;
    private Cancellable mIceExchangeTask;
    private boolean mIceExchangeRequested = false;
    private IceExchangeHandler mPendingIceExchangeRequest;

    private int mExchangeDelayMs = -1;

    protected int mInitialIceExchangeDelayMs = 200;
    protected int mMaxIceExchangeDelayMs = 5000;

    private final Map<Integer, SocketTunnelClient> mPendingTunnel =
            new HashMap<Integer, SocketTunnelClient>();

    public ClientSession(SessionDependencyFactory factory,
                         Executor executor,
                         ServerSessionInterface server,
                         String clientSocketName) throws IOException {
        super(factory, executor, new SocketTunnelClient(clientSocketName));
        mServer = server;
    }

    public void start(RTCConfiguration config) {
        checkCalledOnSessionThread();

        super.start(config, new ServerMessageHandler());
        mConfig = config;

        for (Map.Entry<Integer, SocketTunnelClient> entry : mPendingTunnel.entrySet()) {
            int channelId = entry.getKey();
            entry.getValue().bind(connection().createDataChannel(channelId));
        }

        connection().createAndSetLocalDescription(
                AbstractPeerConnection.SessionDescriptionType.OFFER);
    }

    @Override
    public void stop() {
        for (SocketTunnelClient tunnel : mPendingTunnel.values())
            tunnel.unbind().dispose();

        if (mIceExchangeTask != null)
            mIceExchangeTask.cancel();

        super.stop();
    }

    @Override
    protected void onLocalDescriptionCreatedAndSet(
            AbstractPeerConnection.SessionDescriptionType type, String offer) {
        assert type == AbstractPeerConnection.SessionDescriptionType.OFFER;
        mServer.startSession(mConfig, offer, new CreateSessionHandler());
        mConfig = null;
    }

    private void onAnswerReceived(String answer) {
        connection().setRemoteDescription(
                AbstractPeerConnection.SessionDescriptionType.ANSWER, answer);
    }

    @Override
    protected void onRemoteDescriptionSet() {
        onSessionNegotiated();
    }

    protected void onSessionNegotiated() {
        assert !isIceExchangeStarted();
        updateIceExchangeStatus();
        assert isIceExchangeStarted();
    }

    @Override
    protected void onControlChannelOpened() {
        assert isIceExchangeStarted();
        updateIceExchangeStatus();
    }

    @Override
    protected void onIceConnectionChange() {
        super.onIceConnectionChange();
        updateIceExchangeStatus();
    }

    private void updateIceExchangeStatus() {
        boolean needed = !isConnected() || !isControlChannelOpened();
        if (needed == isIceExchangeStarted())
            return;
        if (needed)
            startIceExchange();
        else
            stopIceExchange();
    }

    private boolean isIceExchangeStarted() {
        return mExchangeDelayMs >= 0;
    }

    private void startIceExchange() {
        assert !isIceExchangeStarted();
        mExchangeDelayMs = mInitialIceExchangeDelayMs;
        startAutoCloseTimer();

        if (!isIceExchangeScheduledOrPending()) {
            scheduleIceExchange(mExchangeDelayMs);
        }

        assert isIceExchangeScheduledOrPending();
        assert isIceExchangeStarted();
    }

    private void stopIceExchange() {
        assert isIceExchangeStarted();
        mExchangeDelayMs = -1;
        stopAutoCloseTimer();

        // Last exchange will happen, not more will be scheduled (unless mIceExchangeRequested).
        assert isIceExchangeScheduledOrPending();
        assert !isIceExchangeStarted();
    }

    private void scheduleIceExchange(int delay) {
        assert mIceExchangeTask == null;
        mIceExchangeTask = postOnSessionThread(delay, new Runnable() {
            @Override
            public void run() {
                mIceExchangeTask = null;

                mServer.iceExchange(takeIceCandidates(), new IceExchangeHandler());
                mIceExchangeRequested = false;
            }
        });
    }

    private boolean isIceExchangeScheduledOrPending() {
        return mIceExchangeTask != null || mPendingIceExchangeRequest != null;
    }

    private void onServerCandidates(List<String> serverCandidates) {
        addIceCandidates(serverCandidates);

        if (isIceExchangeStarted()) {
            mExchangeDelayMs *= 2;
            if (mExchangeDelayMs > mMaxIceExchangeDelayMs) {
                mExchangeDelayMs = mMaxIceExchangeDelayMs;
            }

            scheduleIceExchange(mExchangeDelayMs);
        } else if (mIceExchangeRequested) {
            scheduleIceExchange(mInitialIceExchangeDelayMs);
        }
    }

    /**
     * Queries single ICE eqchange cycle regardless of ICE exchange process.
     */
    private void queryIceExchange() {
        mIceExchangeRequested = true;
        if (mIceExchangeTask == null && mPendingIceExchangeRequest != null) {
            assert !isIceExchangeStarted();
            scheduleIceExchange(mInitialIceExchangeDelayMs);
        }
    }

    private final class CreateSessionHandler implements NegotiationCallback {
        @Override
        public void onSuccess(String answer) {
            checkCalledOnSessionThread();

            onAnswerReceived(answer);
        }

        @Override
        public void onFailure(String message) {
            checkCalledOnSessionThread();

            ClientSession.this.onFailure(message);
        }
    }

    private final class IceExchangeHandler implements IceExchangeCallback {
        public IceExchangeHandler() {
            assert mPendingIceExchangeRequest == null;
            mPendingIceExchangeRequest = this;
        }

        @Override
        public void onSuccess(List<String> serverCandidates) {
            checkCalledOnSessionThread();

            mPendingIceExchangeRequest = null;
            if (isStarted()) {
                onServerCandidates(serverCandidates);
            }
        }

        @Override
        public void onFailure(String message) {
            checkCalledOnSessionThread();

            mPendingIceExchangeRequest = null;
            if (isStarted()) {
                ClientSession.this.onFailure(message);
            }
        }
    }

    private final class ServerMessageHandler extends SessionControlMessages.ServerMessageHandler {
        @Override
        protected void onMessage(SessionControlMessages.ServerMessage message) {
            switch (message.type) {
                case ICE_EXCHANGE:
                    queryIceExchange();
                    break;

                case UNKNOWN_RESPONSE:
                    onUnknownResponse((SessionControlMessages.UnknownResponseMessage) message);
                    break;
            }
        }
    }

    private void onUnknownResponse(SessionControlMessages.UnknownResponseMessage message) {
        // TODO(serya): Handle server version incompatibility.
    }

    @Override
    protected void sendControlMessage(SessionControlMessages.Message<?> message) {
        assert message instanceof SessionControlMessages.ClientMessage;
        super.sendControlMessage(message);
    }
}
