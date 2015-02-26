// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.util.Log;

/**
 * Represents the MessageChannel MessagePort object. Inspired from
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/web-messaging.html#message-channels
 *
 * State management:
 *
 * Initially a message port will be in a pending state. It will be ready once it is created in
 * content/ (in IO thread) and a message port id is assigned.
 * A pending message port cannnot be transferred, and cannot send or receive messages. However,
 * these details are hidden from the user. If a message port is in the pending state:
 * 1. Any messages posted in this port will be queued until the port is ready
 * 2. Transferring the port using a message channel will cause the message (and any subsequent
 *    messages sent) to be queued until the port is ready
 * 3. Transferring the pending port via postMessageToFrame will cause the message (and all
 *    subsequent messages posted via postMessageToFrame) to be queued until the port is ready.
 *
 * A message port can be in transferred state while a transfer is pending or complete. An
 * application cannot use a transferred port to post messages. If a transferred port
 * receives messages, they will be queued. This state is not visible to embedder app.
 *
 * A message port should be closed by the app when it is not needed any more. This will free
 * any resources used by it. A closed port cannot receive/send messages and cannot be transferred.
 * close() can be called multiple times. A transferred port cannot be closed by the application,
 * since the ownership is also transferred during the transfer. Closing a transferred port will
 * throw an exception.
 *
 * TODO(sgurun) implement queueing messages while a port is in transfer
 */
public class MessagePort implements PostMessageSender.PostMessageSenderDelegate {

    /**
     * The message event handler for receiving messages. Called on a background thread.
     */
    public abstract static class WebEventHandler {
        public abstract void onMessage(String message);
    };

    private static final String TAG = "MessagePort";
    private static final int PENDING = -1;
    private int mPortId = PENDING;
    private WebEventHandler mWebEventHandler;
    private AwMessagePortService mMessagePortService;
    private boolean mClosed;
    private boolean mTransferred;
    private PostMessageSender mPostMessageSender;

    public MessagePort(AwMessagePortService messagePortService) {
        mMessagePortService = messagePortService;
        mPostMessageSender = new PostMessageSender(this, mMessagePortService);
        mMessagePortService.addObserver(mPostMessageSender);
    }

    public boolean isReady() {
        return mPortId != PENDING;
    }

    public int portId() {
        return mPortId;
    }

    public void setPortId(int id) {
        mPortId = id;
    }

    public void close() {
        if (mTransferred) {
            throw new IllegalStateException("Port is already transferred");
        }
        if (mClosed) return;
        mClosed = true;
        // If the port is already ready, and no messages are waiting in the
        // queue to be transferred, onPostMessageQueueEmpty() callback is not
        // received (it is received only after messages are purged). In this
        // case do the cleanup here.
        if (isReady() && mPostMessageSender.isMessageQueueEmpty()) {
            cleanup();
        }
    }

    public boolean isClosed() {
        return mClosed;
    }

    public boolean isTransferred() {
        return mTransferred;
    }

    public void setTransferred() {
        mTransferred = true;
    }

    public void setWebEventHandler(WebEventHandler webEventHandler) {
        mWebEventHandler = webEventHandler;
    }

    public void onMessage(String message) {
        if (isClosed()) {
            Log.w(TAG, "Port [" + mPortId + "] received message in closed state");
            return;
        }
        if (mWebEventHandler == null) {
            Log.w(TAG, "No handler set for port [" + mPortId + "], dropping message " + message);
            return;
        }
        mWebEventHandler.onMessage(message);
    }

    public void postMessage(String message, MessagePort[] msgPorts) throws IllegalStateException {
        if (isClosed() || isTransferred()) {
            throw new IllegalStateException("Port is already closed or transferred");
        }
        if (msgPorts != null) {
            for (MessagePort port : msgPorts) {
                if (port.equals(this)) {
                    throw new IllegalStateException("Source port cannot be transferred");
                }
            }
        }
        mPostMessageSender.postMessage(null, message, null, msgPorts);
    }

    // Implements PostMessageSender.PostMessageSenderDelegate interface method.
    @Override
    public boolean isPostMessageSenderReady() {
        return isReady();
    }

    // Implements PostMessageSender.PostMessageSenderDelegate interface method.
    @Override
    public void onPostMessageQueueEmpty() {
        if (isClosed()) {
            cleanup();
        }
    }

    // Implements PostMessageSender.PostMessageSenderDelegate interface method.
    @Override
    public void postMessageToWeb(String frameName, String message, String targetOrigin,
            int[] sentPortIds) {
        mMessagePortService.postMessage(mPortId, message, sentPortIds);
    }

    private void cleanup() {
        mMessagePortService.removeObserver(mPostMessageSender);
        mPostMessageSender = null;
        mMessagePortService.closePort(mPortId);
    }
}
