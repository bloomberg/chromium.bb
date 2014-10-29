// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import java.util.List;

/**
 * Mock of SignalingReceiver.
 */
public class SignalingReceiverMock implements SignalingReceiver {
    public String sessionId;
    public String offer;
    public SessionBase.NegotiationCallback negotiationCallback;
    public SessionBase.IceExchangeCallback iceExchangeCallback;

    @Override
    public void startSession(
            String sessionId,
            RTCConfiguration config,
            String offer,
            SessionBase.NegotiationCallback callback) {
        this.sessionId = sessionId;
        this.offer = offer;
        this.negotiationCallback = callback;
    }

    @Override
    public void renegotiate(
            String sessionId,
            String offer,
            SessionBase.NegotiationCallback callback) {
        this.sessionId = sessionId;
        this.offer = offer;
        this.negotiationCallback = callback;
    }

    @Override
    public void iceExchange(
            String sessionId,
            List<String> clientCandidates,
            SessionBase.IceExchangeCallback callback) {
        this.sessionId = sessionId;
        this.iceExchangeCallback = callback;
    }
}
