// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
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
 * The fact that messages can be handled on a separate thread means that thread
 * synchronization is important. All methods are called on UI thread except as noted.
 *
 * Restrictions:
 * The HTML5 message protocol is very flexible in transferring ports. However, this
 * sometimes leads to surprising behavior. For example, in current version of chrome (m41)
 * the code below
 *  1.  var c1 = new MessageChannel();
 *  2.  var c2 = new MessageChannel();
 *  3.  c1.port2.onmessage= function(e) { console.log("1"); }
 *  4.  c2.port2.onmessage = function(e) {
 *  5.     e.ports[0].onmessage = function(f) {
 *  6.          console.log("3");
 *  7.      }
 *  8.  }
 *  9.  c1.port1.postMessage("test");
 *  10. c2.port1.postMessage("test2",[c1.port2])
 *
 * prints 1 or 3 depending on whether or not line 10 is included in code. Further if
 * it gets executed with a timeout, depending on timeout value, the printout value
 * changes.
 *
 * To prevent such problems, Android webview implementation limits the transfer of ports
 * as below:
 * Webview puts a port to a "started" state if:
 * 1. The port is ever used to post a message, or
 * 2. The port was ever registered a handler to receive a message.
 * A started port cannot be transferred.
 *
 * This restriction should not impact postmessage functionality in a big way,
 * because an app can still create as many channels as it wants to and use it for
 * transferring data. As a return, it simplifies implementation and prevents hard
 * to debug, racy corner cases while receiving/sending data.
 */
public class MessagePort implements PostMessageSender.PostMessageSenderDelegate {

    /**
     * The message event handler for receiving messages. Called on a background thread.
     */
    public abstract static class WebEventHandler {
        public abstract void onMessage(String message);
    }

    private static final String TAG = "MessagePort";
    private static final int PENDING = -1;

    // the what value for POST_MESSAGE
    private static final int POST_MESSAGE = 1;

    private static class PostMessageFromWeb {
        public MessagePort port;
        public String message;

        public PostMessageFromWeb(MessagePort port, String message) {
            this.port = port;
            this.message = message;
        }
    }

    // Implements the handler to handle messageport messages received from web.
    // These messages are received on IO thread and normally handled in main
    // thread however, alternatively application can pass a handler to execute them.
    private static class MessageHandler extends Handler {
        public MessageHandler(Looper looper) {
            super(looper);
        }
        @Override
        public void handleMessage(Message msg) {
            if (msg.what == POST_MESSAGE) {
                PostMessageFromWeb m = (PostMessageFromWeb) msg.obj;
                m.port.onMessage(m.message);
                return;
            }
            throw new IllegalStateException("undefined message");
        }
    }
    // The default message handler
    private static final MessageHandler sDefaultHandler =
            new MessageHandler(Looper.getMainLooper());

    private int mPortId = PENDING;
    private WebEventHandler mWebEventHandler;
    private AwMessagePortService mMessagePortService;
    private boolean mClosed;
    private boolean mTransferred;
    private boolean mStarted;
    private boolean mReleasedMessages;
    private PostMessageSender mPostMessageSender;
    private MessageHandler mHandler;
    private Object mLock = new Object();

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
        releaseMessages();
    }

    public void close() {
        if (mTransferred) {
            throw new IllegalStateException("Port is already transferred");
        }
        synchronized (mLock) {
            if (mClosed) return;
            mClosed = true;
        }
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

    public boolean isStarted() {
        return mStarted;
    }

    // Only called on UI thread
    public void setWebEventHandler(WebEventHandler webEventHandler, Handler handler) {
        mStarted = true;
        synchronized (mLock) {
            mWebEventHandler = webEventHandler;
            if (handler != null) {
                mHandler = new MessageHandler(handler.getLooper());
            }
        }
        releaseMessages();
    }

    // Only called on IO thread.
    public void onReceivedMessage(String message) {
        synchronized (mLock) {
            PostMessageFromWeb m = new PostMessageFromWeb(this, message);
            Handler handler = mHandler != null ? mHandler : sDefaultHandler;
            Message msg = handler.obtainMessage(POST_MESSAGE, m);
            handler.sendMessage(msg);
        }
    }

    private void releaseMessages() {
        if (mReleasedMessages || !isReady() || mWebEventHandler == null) {
            return;
        }
        mReleasedMessages = true;
        mMessagePortService.releaseMessages(mPortId);
    }

    // This method may be called on a different thread than UI thread.
    public void onMessage(String message) {
        synchronized (mLock) {
            if (isClosed()) {
                Log.w(TAG, "Port [" + mPortId + "] received message in closed state");
                return;
            }
            if (mWebEventHandler == null) {
                Log.w(TAG, "No handler set for port [" + mPortId + "], dropping message "
                        + message);
                return;
            }
            mWebEventHandler.onMessage(message);
        }
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
        mStarted = true;
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
