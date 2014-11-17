// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import java.util.concurrent.CountDownLatch;

/**
 * Tests for {@link SessionDependencyFactoryNative}
 */
public class SessionDependencyFactoryNativeTest extends InstrumentationTestCase {
    private SessionDependencyFactoryNative mInstance;
    private AbstractPeerConnection mConnection;
    private ObserverMock mObserver;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        System.loadLibrary("devtools_bridge_natives_so");
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

    private SessionDependencyFactoryNative newFactory() {
        return new SessionDependencyFactoryNative();
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

        final AbstractPeerConnection mConnection1;
        final AbstractPeerConnection mConnection2;

        final AbstractDataChannel mDataChannel1;
        final AbstractDataChannel mDataChannel2;

        Pipe(SessionDependencyFactoryNative factory) {
            RTCConfiguration config = new RTCConfiguration();
            mConnection1 = factory.createPeerConnection(config, mObserver1);
            mConnection2 = factory.createPeerConnection(config, mObserver2);

            mObserver1.iceCandidatesSink = mConnection2;
            mObserver2.iceCandidatesSink = mConnection1;

            mDataChannel1 = mConnection1.createDataChannel(0);
            mDataChannel2 = mConnection2.createDataChannel(1);
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
}
