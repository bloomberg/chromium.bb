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

    private static final String TAG = "AwMessagePortService";

    private int mPortId;
    private MessageHandler mHandler;

    public MessagePort(int portId) {
        mPortId = portId;
    }

    public int portId() {
        return mPortId;
    }

    public void setMessageHandler(MessageHandler handler) {
        mHandler = handler;
    }

    public void onMessage(String message) {
        if (mHandler == null) {
            Log.w(TAG, "No handler set for port [" + mPortId + "], dropping message " + message);
            return;
        }
        mHandler.onMessage(message);
    }
}
