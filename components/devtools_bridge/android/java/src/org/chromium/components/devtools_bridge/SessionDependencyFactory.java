// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import org.webrtc.DataChannel;
import org.webrtc.IceCandidate;
import org.webrtc.MediaConstraints;
import org.webrtc.MediaStream;
import org.webrtc.PeerConnection;
import org.webrtc.PeerConnectionFactory;
import org.webrtc.SdpObserver;
import org.webrtc.SessionDescription;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

/**
 * Implements AbstractDataChannel and AbstractPeerConnection on top of org.webrtc.* API.
 * Isolation is needed because some configuration of DevTools bridge may not be based on
 * Java API. Native implementation of SessionDependencyFactory will be added for this case.
 * In addition abstraction layer isolates SessionBase from complexity of underlying API
 * beside used features.
 */
public class SessionDependencyFactory {
    private final PeerConnectionFactory mFactory = new PeerConnectionFactory();

    public AbstractPeerConnection createPeerConnection(
            RTCConfiguration config, AbstractPeerConnection.Observer observer) {
        MediaConstraints constraints = new MediaConstraints();
        constraints.mandatory.add(
                new MediaConstraints.KeyValuePair("DtlsSrtpKeyAgreement", "true"));
        return new PeerConnectionAdapter(
                mFactory.createPeerConnection(convert(config), constraints,
                        new PeerConnnectionObserverAdapter(observer)), observer);
    }

    public void dispose() {
        mFactory.dispose();
    }

    private static AbstractPeerConnection.SessionDescriptionType convertType(
            SessionDescription.Type type) {
        switch (type) {
            case OFFER:
                return AbstractPeerConnection.SessionDescriptionType.OFFER;
            case ANSWER:
                return AbstractPeerConnection.SessionDescriptionType.ANSWER;
            default:
                throw new IllegalArgumentException(type.toString());
        }
    }

    private static SessionDescription.Type convertType(
            AbstractPeerConnection.SessionDescriptionType type) {
        switch (type) {
            case OFFER:
                return SessionDescription.Type.OFFER;
            case ANSWER:
                return SessionDescription.Type.ANSWER;
            default:
                throw new IllegalArgumentException(type.toString());
        }
    }

    private static AbstractPeerConnection.IceCandidate convert(IceCandidate candidate) {
        return new AbstractPeerConnection.IceCandidate(
                candidate.sdpMid, candidate.sdpMLineIndex, candidate.sdp);
    }

    private static IceCandidate convert(AbstractPeerConnection.IceCandidate candidate) {
        return new IceCandidate(candidate.sdpMid, candidate.sdpMLineIndex, candidate.sdp);
    }

    private static List<PeerConnection.IceServer> convert(RTCConfiguration config) {
        List<PeerConnection.IceServer> result = new ArrayList<PeerConnection.IceServer>();
        for (RTCConfiguration.IceServer server : config.iceServers) {
            result.add(new PeerConnection.IceServer(
                    server.uri, server.username, server.credential));
        }
        return result;
    }

    public static DataChannelAdapter createDataChannel(PeerConnection connection, int channelId) {
        DataChannel.Init init = new DataChannel.Init();
        init.ordered = true;
        init.negotiated = true;
        init.id = channelId;
        return new DataChannelAdapter(connection.createDataChannel("", init));
    }

    private static final class DataChannelAdapter extends AbstractDataChannel {
        private final DataChannel mAdaptee;

        public DataChannelAdapter(DataChannel adaptee) {
            mAdaptee = adaptee;
        }

        @Override
        public void dispose() {
            mAdaptee.dispose();
        }

        @Override
        public void close() {
            mAdaptee.close();
        }

        @Override
        public void send(ByteBuffer message, AbstractDataChannel.MessageType type) {
            assert message.remaining() > 0;
            mAdaptee.send(new DataChannel.Buffer(
                    message, type == AbstractDataChannel.MessageType.BINARY));
        }

        @Override
        public void registerObserver(Observer observer) {
            mAdaptee.registerObserver(new DataChannelObserverAdapter(observer, mAdaptee));
        }

        @Override
        public void unregisterObserver() {
            mAdaptee.unregisterObserver();
        }
    }

    private static final class DataChannelObserverAdapter implements DataChannel.Observer {
        private final AbstractDataChannel.Observer mAdaptee;
        private final DataChannel mDataChannel;
        private AbstractDataChannel.State mState = AbstractDataChannel.State.CLOSED;

        public DataChannelObserverAdapter(
                AbstractDataChannel.Observer adaptee, DataChannel dataChannel) {
            mAdaptee = adaptee;
            mDataChannel = dataChannel;
        }

        @Override
        public void onStateChange() {
            AbstractDataChannel.State state = mDataChannel.state() == DataChannel.State.OPEN ?
                    AbstractDataChannel.State.OPEN : AbstractDataChannel.State.CLOSED;
            if (mState != state) {
                mState = state;
                mAdaptee.onStateChange(state);
            }
        }

        @Override
        public void onMessage(DataChannel.Buffer buffer) {
            assert buffer.data.remaining() > 0;
            mAdaptee.onMessage(buffer.data);
        }
    }

    private abstract static class SetHandler implements SdpObserver {
        @Override
        public final void onCreateSuccess(SessionDescription description) {
            assert false;
        }

        @Override
        public final void onCreateFailure(String error) {
            assert false;
        }
    }

    private abstract static class CreateHandler implements SdpObserver {
        @Override
        public final void onSetSuccess() {
            assert false;
        }

        @Override
        public final void onSetFailure(String error) {
            assert false;
        }
    }

    private static final class CreateAndSetHandler extends CreateHandler {
        private final PeerConnectionAdapter mConnection;
        private final AbstractPeerConnection.Observer mObserver;

        public CreateAndSetHandler(PeerConnectionAdapter connection,
                                   AbstractPeerConnection.Observer observer) {
            mConnection = connection;
            mObserver = observer;
        }

        @Override
        public void onCreateSuccess(final SessionDescription localDescription) {
            mConnection.setLocalDescriptionOnSignalingThread(localDescription);
        }

        @Override
        public void onCreateFailure(String description) {
            mObserver.onFailure(description);
        }
    }

    private static final class LocalSetHandler extends SetHandler {
        private final SessionDescription mLocalDescription;
        private final AbstractPeerConnection.Observer mObserver;

        public LocalSetHandler(SessionDescription localDescription,
                               AbstractPeerConnection.Observer observer) {
            mLocalDescription = localDescription;
            mObserver = observer;
        }

        @Override
        public void onSetSuccess() {
            mObserver.onLocalDescriptionCreatedAndSet(
                    convertType(mLocalDescription.type), mLocalDescription.description);
        }

        @Override
        public void onSetFailure(String description) {
            mObserver.onFailure(description);
        }
    }

    private static final class SetRemoteDescriptionHandler extends SetHandler {
        private final AbstractPeerConnection.Observer mObserver;

        public SetRemoteDescriptionHandler(AbstractPeerConnection.Observer observer) {
            mObserver = observer;
        }

        @Override
        public void onSetSuccess() {
            mObserver.onRemoteDescriptionSet();
        }

        @Override
        public void onSetFailure(String description) {
            mObserver.onFailure(description);
        }
    }

    private static final class PeerConnectionAdapter extends AbstractPeerConnection {
        private PeerConnection mAdaptee;
        private final Observer mObserver;

        // Only access from signaling thread and disposing need synchronization.
        private final Object mDisposeLock = new Object();

        public PeerConnectionAdapter(PeerConnection adaptee, Observer observer) {
            mAdaptee = adaptee;
            mObserver = observer;
        }

        public void setLocalDescriptionOnSignalingThread(SessionDescription description) {
            synchronized (mDisposeLock) {
                if (mAdaptee == null)
                    return;

                mAdaptee.setLocalDescription(
                        new LocalSetHandler(description, mObserver), description);
            }
        }

        @Override
        public void createAndSetLocalDescription(SessionDescriptionType type) {
            CreateAndSetHandler handler = new CreateAndSetHandler(this, mObserver);
            switch (type) {
                case OFFER:
                    mAdaptee.createOffer(handler, new MediaConstraints());
                    break;

                case ANSWER:
                    mAdaptee.createAnswer(handler, new MediaConstraints());
                    break;

                default:
                    assert false;
            }
        }

        @Override
        public void setRemoteDescription(SessionDescriptionType type, String description) {
            mAdaptee.setRemoteDescription(new SetRemoteDescriptionHandler(mObserver),
                                          new SessionDescription(convertType(type), description));
        }

        @Override
        public void addIceCandidate(String candidate) {
            mAdaptee.addIceCandidate(convert(
                    AbstractPeerConnection.IceCandidate.fromString(candidate)));
        }

        @Override
        public void dispose() {
            synchronized (mDisposeLock) {
                mAdaptee.dispose();
                mAdaptee = null;
            }
        }

        @Override
        public AbstractDataChannel createDataChannel(int channelId) {
            DataChannel.Init init = new DataChannel.Init();
            init.ordered = true;
            init.negotiated = true;
            init.id = channelId;
            return new DataChannelAdapter(mAdaptee.createDataChannel("", init));
        }
    }

    private static final class PeerConnnectionObserverAdapter implements PeerConnection.Observer {
        private final AbstractPeerConnection.Observer mAdaptee;
        private boolean mConnected = false;

        public PeerConnnectionObserverAdapter(AbstractPeerConnection.Observer adaptee) {
            mAdaptee = adaptee;
        }

        @Override
        public void onIceCandidate(IceCandidate candidate) {
            mAdaptee.onIceCandidate(convert(candidate).toString());
        }

        @Override
        public void onSignalingChange(PeerConnection.SignalingState newState) {}

        @Override
        public void onIceConnectionChange(PeerConnection.IceConnectionState newState) {
            boolean connected = isConnected(newState);
            if (mConnected != connected) {
                mConnected = connected;
                mAdaptee.onIceConnectionChange(connected);
            }
        }

        private static boolean isConnected(PeerConnection.IceConnectionState newState) {
            switch (newState) {
                case CONNECTED:
                case COMPLETED:
                    return true;
                default:
                    return false;
            }
        }

        @Override
        public void onIceGatheringChange(PeerConnection.IceGatheringState newState) {}

        @Override
        public void onDataChannel(DataChannel dataChannel) {
            // Remote peer added non-prenegotiated data channel. It's not supported.
            dataChannel.dispose();
        }

        @Override
        public void onAddStream(MediaStream stream) {}

        @Override
        public void onRemoveStream(MediaStream stream) {}

        @Override
        public void onRenegotiationNeeded() {}
    }
}
