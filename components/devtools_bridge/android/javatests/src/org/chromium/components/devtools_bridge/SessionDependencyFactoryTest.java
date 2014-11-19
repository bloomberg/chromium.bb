// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import java.nio.ByteBuffer;

/**
 * Tests for {@link SessionDependencyFactory}
 */
public class SessionDependencyFactoryTest extends InstrumentationTestCase {
    private static final int DATA_CHANNEL_ID = 0;

    private SessionDependencyFactory mInstance;
    private AbstractPeerConnection mConnection;
    private PeerConnectionObserverMock mObserver;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mObserver = new PeerConnectionObserverMock();
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
        DataPipe pipe = new DataPipe(mInstance);
        pipe.negotiate();
        pipe.dispose();
        mInstance.dispose();
    }

    @SmallTest
    public void testConnection() throws Exception {
        mInstance = newFactory();
        DataPipe pipe = new DataPipe(mInstance);
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

        channel.registerObserver(new DataChannelObserverMock());
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
        DataPipe pipe = new DataPipe(mInstance);

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
        DataPipe pipe = new DataPipe(mInstance);
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

    private AbstractPeerConnection newConnection(PeerConnectionObserverMock observer) {
        return mInstance.createPeerConnection(new RTCConfiguration(), observer);
    }
}
