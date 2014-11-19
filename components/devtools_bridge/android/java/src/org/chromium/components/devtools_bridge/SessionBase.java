// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Base class for ServerSession and ClientSession. Both opens a control channel and a default
 * tunnel. Control channel designated to exchange messages defined in SessionControlMessages.
 *
 * Signaling communication between client and server works in request/response manner. It's more
 * restrictive than traditional bidirectional signaling channel but give more freedom in
 * implementing signaling. Main motivation is that GCD provides API what works in that way.
 *
 * Session is initiated by a client. It creates an offer and sends it along with RTC configuration.
 * Server sends an answer in response. Once session negotiated client starts ICE candidates
 * exchange. It periodically sends own candidates and peeks server's ones. Periodic ICE exchange
 * stops when control channel opens. It resumes if connections state turns to DISCONNECTED (because
 * server may generate ICE candidates to recover connectivity but may not notify through
 * control channel). ICE exchange in CONNECTED state designated to let improve connection
 * when network configuration changed.
 *
 * If session is not started (or resumed) after mAutoCloseTimeoutMs it closes itself.
 *
 * Only default tunnel is supported at the moment. It designated for DevTools UNIX socket.
 * Additional tunnels may be useful for: 1) reverse port forwarding and 2) tunneling
 * WebView DevTools sockets of other applications. Additional tunnels negotiation should
 * be implemented by adding new types of control messages. Dynamic tunnel configuration
 * will need support for session renegotiation.
 *
 * Session is a single threaded object. Until started owner is responsible to synchronizing access
 * to it. When started it must be called on the thread of SessionBase.Executor.
 * All WebRTC callbacks are forwarded on this thread.
 */
public abstract class SessionBase {
    private static final int CONTROL_CHANNEL_ID = 0;
    private static final int DEFAULT_TUNNEL_CHANNEL_ID = 1;

    private final Executor mExecutor;
    protected final SessionDependencyFactory mFactory;
    private AbstractPeerConnection mConnection;
    private AbstractDataChannel mControlChannel;
    private List<String> mCandidates = new ArrayList<String>();
    private boolean mControlChannelOpened = false;
    private boolean mConnected = false;
    private Cancellable mAutoCloseTask;
    private SessionControlMessages.MessageHandler mControlMessageHandler;
    private final Map<Integer, SocketTunnel> mTunnels =
            new HashMap<Integer, SocketTunnel>();
    private EventListener mEventListener;

    protected int mAutoCloseTimeoutMs = 30000;

    /**
     * Allows to post tasks on the thread where the sessions lives.
     */
    public interface Executor {
        Cancellable postOnSessionThread(int delayMs, Runnable runnable);
        boolean isCalledOnSessionThread();
    }

    /**
     * Interface for cancelling scheduled tasks.
     */
    public interface Cancellable {
        void cancel();
    }

    /**
     * Representation of server session. All methods are delivered through
     * signaling channel (except test configurations). Server session is accessible
     * in request/response manner.
     */
    public interface ServerSessionInterface {
        /**
         * Starts session with specified RTC configuration and offer.
         */
        void startSession(RTCConfiguration config,
                          String offer,
                          NegotiationCallback callback);

        /**
         * Renegoteates session. Needed when tunnels added/removed on the fly.
         */
        void renegotiate(String offer, NegotiationCallback callback);

        /**
         * Sends client's ICE candidates to the server and peeks server's ICE candidates.
         */
        void iceExchange(List<String> clientCandidates, IceExchangeCallback callback);
    }

    /**
     * Base interface for server callbacks.
     */
    public interface ServerCallback {
        void onFailure(String errorMessage);
    }

    /**
     * Server's response to startSession and renegotiate methods.
     */
    public interface NegotiationCallback extends ServerCallback {
        void onSuccess(String answer);
    }

    /**
     * Server's response on iceExchange method.
     */
    public interface IceExchangeCallback  extends ServerCallback {
        void onSuccess(List<String> serverCandidates);
    }

    /**
     * Listener of session's events.
     */
    public interface EventListener {
        void onCloseSelf();
    }

    protected SessionBase(SessionDependencyFactory factory,
                          Executor executor,
                          SocketTunnel defaultTunnel) {
        mExecutor = executor;
        mFactory = factory;
        addTunnel(DEFAULT_TUNNEL_CHANNEL_ID, defaultTunnel);
    }

    public final void dispose() {
        checkCalledOnSessionThread();

        if (isStarted()) stop();

        for (SocketTunnel tunnel : mTunnels.values()) {
            tunnel.dispose();
        }
    }

    public void setEventListener(EventListener listener) {
        checkCalledOnSessionThread();

        mEventListener = listener;
    }

    protected AbstractPeerConnection connection() {
        return mConnection;
    }

    protected boolean doesTunnelExist(int channelId) {
        return mTunnels.containsKey(channelId);
    }

    private final void addTunnel(int channelId, SocketTunnel tunnel) {
        assert !mTunnels.containsKey(channelId);
        assert !tunnel.isBound();
        // Tunnel renegotiation not implemented.
        assert channelId == DEFAULT_TUNNEL_CHANNEL_ID && !isStarted();

        mTunnels.put(channelId, tunnel);
    }

    protected void removeTunnel(int channelId) {
        assert mTunnels.containsKey(channelId);
        mTunnels.get(channelId).unbind().dispose();
        mTunnels.remove(channelId);
    }

    protected final boolean isControlChannelOpened() {
        return mControlChannelOpened;
    }

    protected final boolean isConnected() {
        return mConnected;
    }

    protected final void postOnSessionThread(Runnable runnable) {
        postOnSessionThread(0, runnable);
    }

    protected final Cancellable postOnSessionThread(int delayMs, Runnable runnable) {
        return mExecutor.postOnSessionThread(delayMs, runnable);
    }

    protected final void checkCalledOnSessionThread() {
        assert mExecutor.isCalledOnSessionThread();
    }

    public final boolean isStarted() {
        return mConnection != null;
    }

    /**
     * Creates and configures peer connection and sets a control message handler.
     */
    protected void start(RTCConfiguration config,
                         SessionControlMessages.MessageHandler handler) {
        assert !isStarted();

        mConnection = mFactory.createPeerConnection(config, new ConnectionObserver());
        mControlChannel = mConnection.createDataChannel(CONTROL_CHANNEL_ID);
        mControlMessageHandler = handler;
        mControlChannel.registerObserver(new ControlChannelObserver());

        for (Map.Entry<Integer, SocketTunnel> entry : mTunnels.entrySet()) {
            int channelId = entry.getKey();
            SocketTunnel tunnel = entry.getValue();
            tunnel.bind(connection().createDataChannel(channelId));
        }
    }

    /**
     * Disposed objects created in |start|.
     */
    public void stop() {
        checkCalledOnSessionThread();

        assert isStarted();

        stopAutoCloseTimer();

        for (SocketTunnel tunnel : mTunnels.values()) {
            tunnel.unbind().dispose();
        }

        AbstractPeerConnection connection = mConnection;
        mConnection = null;
        assert !isStarted();

        mControlChannel.unregisterObserver();
        mControlMessageHandler = null;
        mControlChannel.dispose();
        mControlChannel = null;

        // Dispose connection after data channels.
        connection.dispose();
    }

    protected abstract void onRemoteDescriptionSet();
    protected abstract void onLocalDescriptionCreatedAndSet(
            AbstractPeerConnection.SessionDescriptionType type, String description);
    protected abstract void onControlChannelOpened();

    protected void onControlChannelClosed() {
        closeSelf();
    }

    protected void onIceConnectionChange() {}

    private void handleFailureOnSignalingThread(final String message) {
        postOnSessionThread(new Runnable() {
            @Override
            public void run() {
                if (isStarted())
                    onFailure(message);
            }
        });
    }

    protected final void startAutoCloseTimer() {
        assert mAutoCloseTask == null;
        assert isStarted();
        mAutoCloseTask = postOnSessionThread(mAutoCloseTimeoutMs, new Runnable() {
            @Override
            public void run() {
                assert isStarted();

                mAutoCloseTask = null;
                closeSelf();
            }
        });
    }

    protected final void stopAutoCloseTimer() {
        if (mAutoCloseTask != null) {
            mAutoCloseTask.cancel();
            mAutoCloseTask = null;
        }
    }

    protected void closeSelf() {
        stop();
        if (mEventListener != null) {
            mEventListener.onCloseSelf();
        }
    }

    // Returns collected candidates (for sending to the remote session) and removes them.
    protected List<String> takeIceCandidates() {
        List<String> result = new ArrayList<String>();
        result.addAll(mCandidates);
        mCandidates.clear();
        return result;
    }

    protected void addIceCandidates(List<String> candidates) {
        for (String candidate : candidates) {
            mConnection.addIceCandidate(candidate);
        }
    }

    protected void onFailure(String message) {
        closeSelf();
    }

    protected void onIceCandidate(String candidate) {
        mCandidates.add(candidate);
    }

    /**
     * Receives callbacks from the peer connection on the signaling thread. Forwards them
     * on the session thread. All session event handling methods assume session started (prevents
     * disposed objects). It drops callbacks it closed.
     */
    private final class ConnectionObserver implements AbstractPeerConnection.Observer {
        @Override
        public void onFailure(final String description) {
            postOnSessionThread(new Runnable() {
                @Override
                public void run() {
                    if (!isStarted()) return;
                    SessionBase.this.onFailure(description);
                }
            });
        }

        @Override
        public void onLocalDescriptionCreatedAndSet(
                final AbstractPeerConnection.SessionDescriptionType type,
                final String description) {
            postOnSessionThread(new Runnable() {
                @Override
                public void run() {
                    if (!isStarted()) return;
                    SessionBase.this.onLocalDescriptionCreatedAndSet(type, description);
                }
            });
        }

        @Override
        public void onRemoteDescriptionSet() {
            postOnSessionThread(new Runnable() {
                @Override
                public void run() {
                    if (!isStarted()) return;
                    SessionBase.this.onRemoteDescriptionSet();
                }
            });
        }

        @Override
        public void onIceCandidate(final String candidate) {
            postOnSessionThread(new Runnable() {
                @Override
                public void run() {
                    if (!isStarted()) return;
                    SessionBase.this.onIceCandidate(candidate);
                }
            });
        }

        @Override
        public void onIceConnectionChange(final boolean connected) {
            postOnSessionThread(new Runnable() {
                @Override
                public void run() {
                    if (!isStarted()) return;
                    mConnected = connected;
                    SessionBase.this.onIceConnectionChange();
                }
            });
        }
    }

    /**
     * Receives callbacks from the control channel. Forwards them on the session thread.
     */
    private final class ControlChannelObserver implements AbstractDataChannel.Observer {
        @Override
        public void onStateChange(final AbstractDataChannel.State state) {
            postOnSessionThread(new Runnable() {
                @Override
                public void run() {
                    if (!isStarted()) return;
                    mControlChannelOpened = state == AbstractDataChannel.State.OPEN;

                    if (mControlChannelOpened) {
                        onControlChannelOpened();
                    } else {
                        onControlChannelClosed();
                    }
                }
            });
        }

        @Override
        public void onMessage(ByteBuffer message) {
            final byte[] bytes = new byte[message.remaining()];
            message.get(bytes);
            postOnSessionThread(new Runnable() {
                @Override
                public void run() {
                    if (!isStarted() || mControlMessageHandler == null) return;

                    try {
                        mControlMessageHandler.readMessage(bytes);
                    } catch (SessionControlMessages.InvalidFormatException e) {
                        // TODO(serya): handle
                    }
                }
            });
        }
    }

    protected void sendControlMessage(SessionControlMessages.Message<?> message) {
        assert mControlChannelOpened;

        byte[] bytes = SessionControlMessages.toByteArray(message);
        ByteBuffer rawMessage = ByteBuffer.allocateDirect(bytes.length);
        rawMessage.put(bytes);

        sendControlMessage(rawMessage);
    }

    private void sendControlMessage(ByteBuffer rawMessage) {
        rawMessage.limit(rawMessage.position());
        rawMessage.position(0);
        mControlChannel.send(rawMessage, AbstractDataChannel.MessageType.TEXT);
    }
}
