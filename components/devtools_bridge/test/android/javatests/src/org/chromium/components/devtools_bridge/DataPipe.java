// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import java.nio.ByteBuffer;

/**
 * Represents a pair of connected AbstractDataChannel's. Sends to one channel
 * come to another and vice versa.
 */
public class DataPipe {
    private static final int DATA_CHANNEL_ID = 0;

    final PeerConnectionObserverMock mObserver1 = new PeerConnectionObserverMock();
    final PeerConnectionObserverMock mObserver2 = new PeerConnectionObserverMock();

    DataChannelObserverMock mDataChannelObserverMock1 = new DataChannelObserverMock();
    DataChannelObserverMock mDataChannelObserverMock2 = new DataChannelObserverMock();

    final AbstractPeerConnection mConnection1;
    final AbstractPeerConnection mConnection2;

    final AbstractDataChannel mDataChannel1;
    final AbstractDataChannel mDataChannel2;

    DataPipe(SessionDependencyFactory factory) {
        RTCConfiguration config = new RTCConfiguration();
        mConnection1 = factory.createPeerConnection(config, mObserver1);
        mConnection2 = factory.createPeerConnection(config, mObserver2);

        mObserver1.iceCandidatesSink = mConnection2;
        mObserver2.iceCandidatesSink = mConnection1;

        mDataChannel1 = mConnection1.createDataChannel(DATA_CHANNEL_ID);
        mDataChannel2 = mConnection2.createDataChannel(DATA_CHANNEL_ID);
    }

    void dispose() {
        mDataChannel1.dispose();
        mDataChannel2.dispose();
        mConnection1.dispose();
        mConnection2.dispose();
    }

    void negotiate() throws Exception {
        mConnection1.createAndSetLocalDescription(
                AbstractPeerConnection.SessionDescriptionType.OFFER);
        mObserver1.localDescriptionAvailable.await();

        mConnection2.setRemoteDescription(
                AbstractPeerConnection.SessionDescriptionType.OFFER,
                mObserver1.localDescription);
        mObserver2.remoteDescriptionSet.await();

        mConnection2.createAndSetLocalDescription(
                AbstractPeerConnection.SessionDescriptionType.ANSWER);
        mObserver2.localDescriptionAvailable.await();

        mConnection1.setRemoteDescription(
                AbstractPeerConnection.SessionDescriptionType.ANSWER,
                mObserver2.localDescription);
        mObserver1.remoteDescriptionSet.await();
    }

    void awaitConnected() throws Exception {
        mObserver1.connected.await();
        mObserver2.connected.await();
    }

    void send(int channelIndex, String data) {
        byte[] bytes = data.getBytes();
        ByteBuffer rawMessage = ByteBuffer.allocateDirect(bytes.length);
        rawMessage.put(bytes);
        rawMessage.limit(rawMessage.position());
        rawMessage.position(0);
        dataChannel(channelIndex).send(rawMessage, AbstractDataChannel.MessageType.TEXT);
    }

    void send(int channelIndex, ByteBuffer rawMessage) {
        dataChannel(channelIndex).send(rawMessage, AbstractDataChannel.MessageType.BINARY);
    }

    AbstractDataChannel dataChannel(int channelIndex) {
        switch (channelIndex) {
            case 0:
                return mDataChannel1;

            case 1:
                return mDataChannel2;

            default:
                throw new ArrayIndexOutOfBoundsException();
        }
    }

    DataChannelObserverMock dataChannelObserver(int channelIndex) {
        switch (channelIndex) {
            case 0:
                return mDataChannelObserverMock1;

            case 1:
                return mDataChannelObserverMock2;

            default:
                throw new ArrayIndexOutOfBoundsException();
        }
    }

    void registerDatatChannelObservers() {
        mDataChannel1.registerObserver(mDataChannelObserverMock1);
        mDataChannel2.registerObserver(mDataChannelObserverMock2);
    }

    void unregisterDatatChannelObservers() {
        mDataChannel1.unregisterObserver();
        mDataChannel2.unregisterObserver();
    }
}
