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

        @Override
        public void registerObserver(Observer observer) {
            throw new UnsupportedOperationException("Not implemented yet");
        }

        @Override
        public void unregisterObserver() {
            throw new UnsupportedOperationException("Not implemented yet");
        }

        @Override
        public void send(ByteBuffer message, MessageType type) {
            throw new UnsupportedOperationException("Not implemented yet");
        }

        @Override
        public void close() {
            throw new UnsupportedOperationException("Not implemented yet");
        }

        @Override
        public void dispose() {
            nativeDestroyDataChannel(mChannelPtr);
        }
    }

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
}
