// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

import java.nio.ByteBuffer;

/**
 * Native implementation of session dependency factory on top of C++
 * libjingle API.
 */
@JNINamespace("devtools_bridge::android")
public class SessionDependencyFactoryNative extends SessionDependencyFactory {
    private final long mFactoryPtr;

    public SessionDependencyFactoryNative() {
        mFactoryPtr = nativeCreateFactory();
        assert mFactoryPtr != 0;
    }

    @Override
    public AbstractPeerConnection createPeerConnection(
            RTCConfiguration config, AbstractPeerConnection.Observer observer) {
        assert config != null;
        assert observer != null;

        long configPtr = nativeCreateConfig();
        for (RTCConfiguration.IceServer server : config.iceServers) {
            nativeAddIceServer(configPtr, server.uri, server.username, server.credential);
        }

        return new PeerConnectionImpl(mFactoryPtr, configPtr, observer);
    }

    @Override
    public SocketTunnel newSocketTunnelServer(String socketBase) {
        return new SocketTunnelServerImpl(mFactoryPtr, socketBase);
    }

    @Override
    public void dispose() {
        nativeDestroyFactory(mFactoryPtr);
    }

    private static final class PeerConnectionImpl extends AbstractPeerConnection {
        private final long mConnectionPtr;

        // Takes ownership on |configPtr|.
        public PeerConnectionImpl(
                long factoryPtr, long configPtr,
                AbstractPeerConnection.Observer observer) {
            mConnectionPtr = nativeCreatePeerConnection(factoryPtr, configPtr, observer);
            assert mConnectionPtr != 0;
        }

        @Override
        public void createAndSetLocalDescription(SessionDescriptionType type) {
            switch (type) {
                case OFFER:
                    nativeCreateAndSetLocalOffer(mConnectionPtr);
                    break;

                case ANSWER:
                    nativeCreateAndSetLocalAnswer(mConnectionPtr);
                    break;
            }
        }

        @Override
        public void setRemoteDescription(SessionDescriptionType type, String description) {
            switch (type) {
                case OFFER:
                    nativeSetRemoteOffer(mConnectionPtr, description);
                    break;

                case ANSWER:
                    nativeSetRemoteAnswer(mConnectionPtr, description);
                    break;
            }
        }

        @Override
        public void addIceCandidate(String candidate) {
            // TODO(serya): Handle IllegalArgumentException exception.
            IceCandidate parsed = IceCandidate.fromString(candidate);
            nativeAddIceCandidate(mConnectionPtr, parsed.sdpMid, parsed.sdpMLineIndex, parsed.sdp);
        }

        @Override
        public void dispose() {
            nativeDestroyPeerConnection(mConnectionPtr);
        }

        @Override
        public AbstractDataChannel createDataChannel(int channelId) {
            return new DataChannelImpl(nativeCreateDataChannel(mConnectionPtr, channelId));
        }
    }

    private static final class DataChannelImpl extends AbstractDataChannel {
        private final long mChannelPtr;

        public DataChannelImpl(long ptr) {
            assert ptr != 0;
            mChannelPtr = ptr;
        }

        long nativePtr() {
            return mChannelPtr;
        }

        @Override
        public void registerObserver(Observer observer) {
            nativeRegisterDataChannelObserver(mChannelPtr, observer);
        }

        @Override
        public void unregisterObserver() {
            nativeUnregisterDataChannelObserver(mChannelPtr);
        }

        @Override
        public void send(ByteBuffer message, MessageType type) {
            assert message.position() == 0;
            int length = message.limit();
            assert length > 0;

            switch (type) {
                case BINARY:
                    nativeSendBinaryMessage(mChannelPtr, message, length);
                    break;

                case TEXT:
                    nativeSendTextMessage(mChannelPtr, message, length);
                    break;
            }
        }

        @Override
        public void close() {
            nativeCloseDataChannel(mChannelPtr);
        }

        @Override
        public void dispose() {
            nativeDestroyDataChannel(mChannelPtr);
        }
    }

    private static class SocketTunnelServerImpl implements SocketTunnel {
        private final String mSocketName;
        private final long mFactoryPtr;
        private DataChannelImpl mDataChannel;
        private long mTunnelPtr;

        public SocketTunnelServerImpl(long factoryPtr, String socketName) {
            mFactoryPtr = factoryPtr;
            mSocketName = socketName;
        }

        @Override
        public void bind(AbstractDataChannel dataChannel) {
            mDataChannel = (DataChannelImpl) dataChannel;
            mTunnelPtr = nativeCreateSocketTunnelServer(
                    mFactoryPtr, mDataChannel.nativePtr(), mSocketName);
        }

        @Override
        public AbstractDataChannel unbind() {
            AbstractDataChannel result = mDataChannel;
            nativeDestroySocketTunnelServer(mTunnelPtr);
            mTunnelPtr = 0;
            mDataChannel = null;
            return result;
        }

        @Override
        public boolean isBound() {
            return mDataChannel != null;
        }

        @Override
        public void dispose() {
            assert !isBound();
        }
    }

    // Peer connection callbacks.

    @CalledByNative
    private static void notifyLocalOfferCreatedAndSetSet(Object observer, String description) {
        ((AbstractPeerConnection.Observer) observer).onLocalDescriptionCreatedAndSet(
                AbstractPeerConnection.SessionDescriptionType.OFFER, description);
    }

    @CalledByNative
    private static void notifyLocalAnswerCreatedAndSetSet(Object observer, String description) {
        ((AbstractPeerConnection.Observer) observer).onLocalDescriptionCreatedAndSet(
                AbstractPeerConnection.SessionDescriptionType.ANSWER, description);
    }

    @CalledByNative
    private static void notifyRemoteDescriptionSet(Object observer) {
        ((AbstractPeerConnection.Observer) observer).onRemoteDescriptionSet();
    }

    @CalledByNative
    private static void notifyConnectionFailure(Object observer, String description) {
        ((AbstractPeerConnection.Observer) observer).onFailure(description);
    }

    @CalledByNative
    private static void notifyIceCandidate(
            Object observer, String sdpMid, int sdpMLineIndex, String sdp) {
        ((AbstractPeerConnection.Observer) observer)
                .onIceCandidate(new AbstractPeerConnection.IceCandidate(
                        sdpMid, sdpMLineIndex, sdp).toString());
    }

    @CalledByNative
    private static void notifyIceConnectionChange(Object observer, boolean connected) {
        ((AbstractPeerConnection.Observer) observer)
                .onIceConnectionChange(connected);
    }

    // Data channel callbacks.

    @CalledByNative
    private static void notifyChannelOpen(Object observer) {
        ((AbstractDataChannel.Observer) observer)
                .onStateChange(AbstractDataChannel.State.OPEN);
    }

    @CalledByNative
    private static void notifyChannelClose(Object observer) {
        ((AbstractDataChannel.Observer) observer)
                .onStateChange(AbstractDataChannel.State.CLOSED);
    }

    @CalledByNative
    private static void notifyMessage(Object observer, ByteBuffer message) {
        ((AbstractDataChannel.Observer) observer)
                .onMessage(message);
    }

    private static native long nativeCreateFactory();
    private static native void nativeDestroyFactory(long factoryPtr);

    private static native long nativeCreateConfig();
    private static native void nativeAddIceServer(
            long configPtr, String uri, String username, String credential);

    // Takes ownership on |configPtr|.
    private static native long nativeCreatePeerConnection(
            long factoryPtr, long configPtr, Object observer);
    private static native void nativeDestroyPeerConnection(long connectionPtr);

    private static native void nativeCreateAndSetLocalOffer(long connectionPtr);
    private static native void nativeCreateAndSetLocalAnswer(long connectionPtr);
    private static native void nativeSetRemoteOffer(long connectionPtr, String description);
    private static native void nativeSetRemoteAnswer(long connectionPtr, String description);
    private static native void nativeAddIceCandidate(
            long peerConnectionPtr, String sdpMid, int sdpMLineIndex, String sdp);

    private static native long nativeCreateDataChannel(long connectionPtr, int channelId);
    private static native void nativeDestroyDataChannel(long channelPtr);

    private static native void nativeRegisterDataChannelObserver(
            long channelPtr, Object observer);
    private static native void nativeUnregisterDataChannelObserver(long channelPtr);
    private static native void nativeSendBinaryMessage(
            long channelPtr, ByteBuffer message, int size);
    private static native void nativeSendTextMessage(long channelPtr, ByteBuffer message, int size);
    private static native void nativeCloseDataChannel(long channelPtr);

    private static native long nativeCreateSocketTunnelServer(
            long factoryPtr, long channelPtr, String socketName);
    private static native void nativeDestroySocketTunnelServer(long tunnelPtr);
}
