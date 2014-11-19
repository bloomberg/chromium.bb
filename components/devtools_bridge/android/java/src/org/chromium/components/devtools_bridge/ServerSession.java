// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import java.util.List;

/**
 * DevTools Bridge server session. Handles connection with a ClientSession.
 * See SessionBase description for more detais.
 */
public class ServerSession extends SessionBase implements SessionBase.ServerSessionInterface {
    private NegotiationCallback mNegotiationCallback;
    private IceExchangeCallback mIceExchangeCallback;
    private boolean mIceEchangeRequested = false;

    protected int mGatheringDelayMs = 200;

    public ServerSession(SessionDependencyFactory factory,
                         Executor executor,
                         String defaultSocketName) {
        super(factory, executor, factory.newSocketTunnelServer(defaultSocketName));
    }

    @Override
    public void stop() {
        super.stop();
        if (mNegotiationCallback != null) {
            mNegotiationCallback.onFailure("Session stopped");
            mNegotiationCallback = null;
        }
        if (mIceExchangeCallback != null) {
            mIceExchangeCallback.onFailure("Session stopped");
            mIceExchangeCallback = null;
        }
    }

    @Override
    public void startSession(RTCConfiguration config,
                             String offer,
                             NegotiationCallback callback) {
        checkCalledOnSessionThread();
        if (isStarted()) {
            callback.onFailure("Session already started");
            return;
        }

        ClientMessageHandler handler = new ClientMessageHandler();
        start(config, handler);

        negotiate(offer, callback);
    }

    @Override
    public void renegotiate(String offer, NegotiationCallback callback) {
        checkCalledOnSessionThread();
        if (!isStarted()) {
            callback.onFailure("Session is not started");
            return;
        }

        callback.onFailure("Not implemented");
    }

    private void negotiate(String offer, NegotiationCallback callback) {
        if (mNegotiationCallback != null) {
            callback.onFailure("Negotiation already in progress");
            return;
        }

        mNegotiationCallback = callback;
        // If success will call onRemoteDescriptionSet.
        connection().setRemoteDescription(
                AbstractPeerConnection.SessionDescriptionType.OFFER, offer);
    }

    protected void onRemoteDescriptionSet() {
        // If success will call onLocalDescriptionCreatedAndSet.
        connection().createAndSetLocalDescription(
                AbstractPeerConnection.SessionDescriptionType.ANSWER);
    }

    @Override
    protected void onLocalDescriptionCreatedAndSet(
            AbstractPeerConnection.SessionDescriptionType type, String description) {
        assert type == AbstractPeerConnection.SessionDescriptionType.ANSWER;

        mNegotiationCallback.onSuccess(description);
        mNegotiationCallback = null;
        onSessionNegotiated();
    }

    protected void onSessionNegotiated() {
        if (!isControlChannelOpened())
            startAutoCloseTimer();
    }

    @Override
    public void iceExchange(List<String> clientCandidates,
                            IceExchangeCallback callback) {
        checkCalledOnSessionThread();
        if (!isStarted()) {
            callback.onFailure("Session disposed");
            return;
        }

        if (mNegotiationCallback != null || mIceExchangeCallback != null) {
            callback.onFailure("Concurrent requests detected");
            return;
        }

        mIceExchangeCallback = callback;
        addIceCandidates(clientCandidates);

        // Give libjingle some time for gathering ice candidates.
        postOnSessionThread(mGatheringDelayMs, new Runnable() {
            @Override
            public void run() {
                if (isStarted())
                    sendIceCandidatesBack();
            }
        });
    }

    private void sendIceCandidatesBack() {
        mIceExchangeCallback.onSuccess(takeIceCandidates());
        mIceExchangeCallback = null;
        mIceEchangeRequested = false;
    }

    @Override
    protected void onControlChannelOpened() {
        stopAutoCloseTimer();
    }

    @Override
    protected void onFailure(String message) {
        if (mNegotiationCallback != null) {
            mNegotiationCallback.onFailure(message);
            mNegotiationCallback = null;
        }
        super.onFailure(message);
    }

    @Override
    protected void onIceCandidate(String candidate) {
        super.onIceCandidate(candidate);
        if (isControlChannelOpened() && !mIceEchangeRequested) {
            // New ICE candidate may improve connection even if control channel operable.
            // If control channel closed client will exchange candidates anyway.
            sendControlMessage(new SessionControlMessages.IceExchangeMessage());
            mIceEchangeRequested = true;
        }
    }

    protected SocketTunnel newSocketTunnelServer(String serverSocketName) {
        return mFactory.newSocketTunnelServer(serverSocketName);
    }

    private final class ClientMessageHandler extends SessionControlMessages.ClientMessageHandler {
        @Override
        protected void onMessage(SessionControlMessages.ClientMessage message) {
            switch (message.type) {
                case UNKNOWN_REQUEST:
                    sendControlMessage(((SessionControlMessages.UnknownRequestMessage) message)
                            .createResponse());
                    break;
            }
        }
    }

    @Override
    protected void sendControlMessage(SessionControlMessages.Message<?> message) {
        assert message instanceof SessionControlMessages.ServerMessage;
        super.sendControlMessage(message);
    }
}
