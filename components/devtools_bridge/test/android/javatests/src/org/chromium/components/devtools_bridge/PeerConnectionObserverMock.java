// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import java.util.concurrent.CountDownLatch;

/**
 * Mock for AbstractPeerConnection.Observer.
 */
public class PeerConnectionObserverMock implements AbstractPeerConnection.Observer {
    public AbstractPeerConnection.SessionDescriptionType localDescriptionType;
    public String localDescription;

    public final CountDownLatch localDescriptionAvailable = new CountDownLatch(1);
    public final CountDownLatch failureAvailable = new CountDownLatch(1);
    public final CountDownLatch remoteDescriptionSet = new CountDownLatch(1);
    public final CountDownLatch connected = new CountDownLatch(1);

    public AbstractPeerConnection iceCandidatesSink;

    @Override
    public void onFailure(String description) {
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
