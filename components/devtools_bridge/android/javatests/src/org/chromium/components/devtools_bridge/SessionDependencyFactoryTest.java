// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import java.nio.ByteBuffer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingDeque;

/**
 * Tests for {@link SessionDependencyFactory}
 */
public class SessionDependencyFactoryTest extends InstrumentationTestCase {
    private static final int DATA_CHANNEL_ID = 0;

    private SessionDependencyFactory mInstance;
    private AbstractPeerConnection mConnection;
    private ObserverMock mObserver;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mObserver = new ObserverMock();
    }

    @SmallTest
    public void testCreateFactory() {
        mInstance = newFactory();
        mInstance.dispose();
    }

    @SmallTest
    public void testCreateConnection() {
        mInstance = newFactory();
        RTCConfiguration config = new RTCConfiguration.Builder()
                .addIceServer("http://expample.org")
                .build();
        mInstance.createPeerConnection(config, mObserver).dispose();
        mInstance.dispose();
    }

    @SmallTest
    public void testCreateAndSetLocalOffer() throws Exception {
        mInstance = newFactory();
        mConnection = newConnection();
        mConnection.createAndSetLocalDescription(
                AbstractPeerConnection.SessionDescriptionType.OFFER);

        mObserver.localDescriptionAvailable.await();

        Assert.assertEquals(
                AbstractPeerConnection.SessionDescriptionType.OFFER,
                mObserver.localDescriptionType);
        mConnection.dispose();
        mInstance.dispose();
    }

    @SmallTest
    public void testTerminateCallback() {
        mInstance = newFactory();
        mConnection = newConnection();
        mConnection.createAndSetLocalDescription(
                AbstractPeerConnection.SessionDescriptionType.OFFER);

        // Do not wait.

        mConnection.dispose();
        mInstance.dispose();
    }

    @SmallTest
    public void testCreateAndSetLocalAnswerFailed() throws Exception {
        mInstance = newFactory();
        mConnection = newConnection();
        // Creating answer without offer set must fail.
        mConnection.createAndSetLocalDescription(
                AbstractPeerConnection.SessionDescriptionType.ANSWER);

        mObserver.failureAvailable.await();

        mConnection.dispose();
        mInstance.dispose();
    }

    @SmallTest
    public void testSetRemoteOffer() throws Exception {
        mInstance = newFactory();
        mConnection = newConnection();
        mConnection.createAndSetLocalDescription(
                AbstractPeerConnection.SessionDescriptionType.OFFER);
        mObserver.localDescriptionAvailable.await();
        String offer = mObserver.localDescription;
        mConnection.dispose();

        mConnection = newConnection();
        mConnection.setRemoteDescription(
                AbstractPeerConnection.SessionDescriptionType.OFFER, offer);
        mObserver.remoteDescriptionSet.await();

        mConnection.dispose();
        mInstance.dispose();
    }

    @SmallTest
    public void testNegotiation() throws Exception {
        mInstance = newFactory();
        Pipe pipe = new Pipe(mInstance);
        pipe.negotiate();
        pipe.dispose();
        mInstance.dispose();
    }

    @SmallTest
    public void testConnection() throws Exception {
        mInstance = newFactory();
        Pipe pipe = new Pipe(mInstance);
        pipe.negotiate();
        pipe.awaitConnected();
        pipe.dispose();
        mInstance.dispose();
    }

    @SmallTest
    public void testDataChannel() {
        mInstance = newFactory();
        mConnection = newConnection();
        AbstractDataChannel channel = mConnection.createDataChannel(DATA_CHANNEL_ID);

        channel.registerObserver(new DataChannelObserver());
        channel.send(ByteBuffer.allocateDirect(1), AbstractDataChannel.MessageType.TEXT);
        channel.send(ByteBuffer.allocateDirect(1), AbstractDataChannel.MessageType.BINARY);
        channel.unregisterObserver();
        channel.close();

        channel.dispose();
        mConnection.dispose();
        mInstance.dispose();
    }

    @SmallTest
    public void testDataChannelOpens() throws Exception {
        mInstance = newFactory();
        Pipe pipe = new Pipe(mInstance);

        pipe.registerDatatChannelObservers();

        pipe.negotiate();

        pipe.dataChannelObserver(0).opened.await();
        pipe.dataChannelObserver(1).opened.await();

        pipe.unregisterDatatChannelObservers();

        pipe.dispose();
        mInstance.dispose();
    }

    @MediumTest
    public void testPumpData() throws Exception {
        mInstance = newFactory();
        Pipe pipe = new Pipe(mInstance);
        pipe.registerDatatChannelObservers();
        pipe.negotiate();
        pipe.dataChannelObserver(0).opened.await();

        // Make sure data channel don't leave local references on stack
        // of signaling thread. References causes failure like
        // "Failed adding to JNI local ref table (has 512 entries)".
        final int count = 1000;

        for (int i = 0; i < count; i++) {
            pipe.send(0, "A");
        }

        for (int i = 0; i < count; i++) {
            pipe.dataChannelObserver(1).received.take();
        }

        pipe.unregisterDatatChannelObservers();
        pipe.dispose();
        mInstance.dispose();
    }

    private SessionDependencyFactory newFactory() {
        return SessionDependencyFactory.newInstance();
    }

    private AbstractPeerConnection newConnection() {
        return newConnection(mObserver);
    }

    private AbstractPeerConnection newConnection(ObserverMock observer) {
        return mInstance.createPeerConnection(new RTCConfiguration(), observer);
    }

    static class Pipe {
        final ObserverMock mObserver1 = new ObserverMock();
        final ObserverMock mObserver2 = new ObserverMock();

        DataChannelObserver mDataChannelObserver1 = new DataChannelObserver();
        DataChannelObserver mDataChannelObserver2 = new DataChannelObserver();

        final AbstractPeerConnection mConnection1;
        final AbstractPeerConnection mConnection2;

        final AbstractDataChannel mDataChannel1;
        final AbstractDataChannel mDataChannel2;

        Pipe(SessionDependencyFactory factory) {
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
            send(channelIndex, data.getBytes(), AbstractDataChannel.MessageType.TEXT);
        }

        void send(int channelIndex, byte[] bytes, AbstractDataChannel.MessageType type) {
            ByteBuffer rawMessage = ByteBuffer.allocateDirect(bytes.length);
            rawMessage.put(bytes);
            rawMessage.limit(rawMessage.position());
            rawMessage.position(0);
            dataChannel(channelIndex).send(rawMessage, type);
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

        DataChannelObserver dataChannelObserver(int channelIndex) {
            switch (channelIndex) {
                case 0:
                    return mDataChannelObserver1;

                case 1:
                    return mDataChannelObserver2;

                default:
                    throw new ArrayIndexOutOfBoundsException();
            }
        }

        void registerDatatChannelObservers() {
            mDataChannel1.registerObserver(mDataChannelObserver1);
            mDataChannel2.registerObserver(mDataChannelObserver2);
        }

        void unregisterDatatChannelObservers() {
            mDataChannel1.unregisterObserver();
            mDataChannel2.unregisterObserver();
        }
    }

    private static class ObserverMock implements AbstractPeerConnection.Observer {
        public AbstractPeerConnection.SessionDescriptionType localDescriptionType;
        public String localDescription;
        public String failureDescription;

        public final CountDownLatch localDescriptionAvailable = new CountDownLatch(1);
        public final CountDownLatch failureAvailable = new CountDownLatch(1);
        public final CountDownLatch remoteDescriptionSet = new CountDownLatch(1);
        public final CountDownLatch connected = new CountDownLatch(1);

        public AbstractPeerConnection iceCandidatesSink;

        @Override
        public void onFailure(String description) {
            failureDescription = description;
            failureAvailable.countDown();
        }

        @Override
        public void onLocalDescriptionCreatedAndSet(
                AbstractPeerConnection.SessionDescriptionType type, String description) {
            localDescriptionType = type;
            localDescription = description;
            localDescriptionAvailable.countDown();
        }

        @Override
        public void onRemoteDescriptionSet() {
            remoteDescriptionSet.countDown();
        }

        @Override
        public void onIceCandidate(String iceCandidate) {
            if (iceCandidatesSink != null)
                iceCandidatesSink.addIceCandidate(iceCandidate);
        }

        @Override
        public void onIceConnectionChange(boolean connected) {
            this.connected.countDown();
        }
    }

    private static class DataChannelObserver implements AbstractDataChannel.Observer {
        public final CountDownLatch opened = new CountDownLatch(1);
        public final CountDownLatch closed = new CountDownLatch(1);
        public final LinkedBlockingDeque<byte[]> received = new LinkedBlockingDeque<byte[]>();

        public void onStateChange(AbstractDataChannel.State state) {
            switch (state) {
                case OPEN:
                    opened.countDown();
                    break;

                case CLOSED:
                    closed.countDown();
                    break;
            }
        }

        public void onMessage(ByteBuffer message) {
            byte[] bytes = new byte[message.remaining()];
            message.get(bytes);
            received.add(bytes);
        }
    }
}
