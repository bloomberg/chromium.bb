// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

/**
 * Limited view on org.webrtc.PeerConnection. Abstraction layer helps with:
 * 1. Allows both native and Java API implementation.
 * 2. Hides unused features.
 * Should be accessed on a single thread.
 */
public abstract class AbstractPeerConnection {
    /**
     * All methods are callen on WebRTC signaling thread.
     */
    public interface Observer {
        /**
         * Called when createAndSetLocalDescription or setRemoteDescription failed.
         */
        void onFailure(String description);

        /**
         * Called when createAndSetLocalDescription succeeded.
         */
        void onLocalDescriptionCreatedAndSet(SessionDescriptionType type, String description);

        /**
         * Called when setRemoteDescription succeeded.
         */
        void onRemoteDescriptionSet();

        /**
         * New ICE candidate available. String representation defined in the IceCandidate class.
         * To be sent to the remote peer connection.
         */
        void onIceCandidate(String iceCandidate);

        /**
         * Called when connected or disconnected. In disconnected state recovery procedure
         * should only rely on signaling channel.
         */
        void onIceConnectionChange(boolean connected);
    }

    /**
     * Type of session description.
     */
    public enum SessionDescriptionType {
        OFFER, ANSWER
    }

    /**
     * The result of this method will be invocation onLocalDescriptionCreatedAndSet
     * or onFailure on the observer. Should not be called when waiting result of
     * setRemoteDescription.
     */
    public abstract void createAndSetLocalDescription(SessionDescriptionType type);

    /**
     * Result of this method will be invocation onRemoteDescriptionSet or onFailure on the observer.
     */
    public abstract void setRemoteDescription(SessionDescriptionType type, String description);

    /**
     * Adds a remote ICE candidate.
     */
    public abstract void addIceCandidate(String candidate);

    /**
     * Destroys native objects. Synchronized with the signaling thread
     * (no observer method called when the connection disposed)
     */
    public abstract void dispose();

    /**
     * Creates prenegotiated SCTP data channel.
     */
    public abstract AbstractDataChannel createDataChannel(int channelId);

    /**
     * Helper class which enforces string representation of an ICE candidate.
     */
    static class IceCandidate {
        public final String sdpMid;
        public final int sdpMLineIndex;
        public final String sdp;

        public IceCandidate(String sdpMid, int sdpMLineIndex, String sdp) {
            this.sdpMid = sdpMid;
            this.sdpMLineIndex = sdpMLineIndex;
            this.sdp = sdp;
        }

        public String toString() {
            return sdpMid + ":" + sdpMLineIndex + ":" + sdp;
        }

        public static IceCandidate fromString(String candidate) throws IllegalArgumentException {
            String[] parts = candidate.split(":", 3);
            if (parts.length != 3)
                throw new IllegalArgumentException("Expected column separated list.");
            return new IceCandidate(parts[0], Integer.parseInt(parts[1]), parts[2]);
        }
    }
}
