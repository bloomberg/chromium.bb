// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import java.util.List;

/**
 * Signaling channel interface. Methods are marshalled through GCM.
 * Direct calling supported for tests.
 *
 * Primary reason of the signaling channel is supporting debugging sessions (so why
 * it looks like SessionBase.ServerSessionInterface with session ids). It also
 * may be used for retrieving information when establishing session is not appropriate.
 */
public interface SignalingReceiver {
    /**
     * Starts new session and assigns sessionId to it. Passes all arguments to
     * the ServerSession object. Session ID must be globay unique (like long random) to
     * avoid conflicts among clients.
     */
    void startSession(
            String sessionId,
            RTCConfiguration config,
            String offer,
            SessionBase.NegotiationCallback callback);

    /**
     * Passes call to the appropriate ServerSession object (if it still exists).
     */
    void renegotiate(
            String sessionId,
            String offer,
            SessionBase.NegotiationCallback callback);

    /**
     * Passes call to the appropriate ServerSession object (if it still exists).
     */
    void iceExchange(
            String sessionId,
            List<String> clientCandidates,
            SessionBase.IceExchangeCallback callback);
}
