// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.util.Log;

/**
 * Represents the MessageChannel MessagePort object. Inspired from
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/web-messaging.html#message-channels
 */
public class MessagePort {

    /**
     * The interface for message handler for receiving messages. Called on a background thread.
     */
    public static interface MessageHandler {
        void onMessage(String message);
    };

    private static final String TAG = "MessagePort";

    private int mPortId;
    private MessageHandler mHandler;
    private AwMessagePortService mMessagePortService;
    // A port is put into a closed state when transferred. Such a port can no longer
    // send or receive messages.
    private boolean mClosed;

    public MessagePort(int portId, AwMessagePortService messagePortService) {
        mPortId = portId;
        mMessagePortService = messagePortService;
    }

    public int portId() {
        return mPortId;
    }

    public void close() {
        assert !mClosed;
        mClosed = true;
    }

    public boolean isClosed() {
        return mClosed;
    }

    public void setMessageHandler(MessageHandler handler) {
        mHandler = handler;
    }

    public void onMessage(String message) {
        if (isClosed()) {
            Log.w(TAG, "Port [" + mPortId + "] received message in closed state");
            return;
        }
        if (mHandler == null) {
            Log.w(TAG, "No handler set for port [" + mPortId + "], dropping message " + message);
            return;
        }
        mHandler.onMessage(message);
    }

    public void postMessage(String message, MessagePort[] msgPorts) throws IllegalStateException {
        if (isClosed()) {
            throw new IllegalStateException("Messageport is already closed");
        }
        int[] portIds = null;
        if (msgPorts != null) {
            portIds = new int[msgPorts.length];
            for (int i = 0; i < msgPorts.length; i++) {
                portIds[i] = msgPorts[i].portId();
            }
        }
        mMessagePortService.postMessage(mPortId, message, portIds);
    }
}
