// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.SparseArray;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;

/**
 * Provides the Message Channel functionality for Android Webview. Specifically
 * manages the message ports that are associated with a message channel and
 * handles posting/receiving messages to/from them.
 * See https://html.spec.whatwg.org/multipage/comms.html#messagechannel for
 * further information on message channels.
 *
 * The message ports have unique IDs. In Android webview implementation,
 * the message ports are only known by their IDs at the native side.
 * At the java side, the embedder deals with MessagePort objects. The mapping
 * from an ID to an object is in AwMessagePortService. AwMessagePortService
 * keeps a strong ref to MessagePort objects until they are closed.
 *
 * Ownership: The Java AwMessagePortService is owned by Java AwBrowserContext.
 * The native AwMessagePortService is owned by native AwBrowserContext. The
 * native peer maintains a weak ref to the java object and deregisters itself
 * before being deleted.
 *
 * All methods are called on UI thread except as noted.
 */
@JNINamespace("android_webview")
public class AwMessagePortService {

    private static final String TAG = "AwMessagePortService";

    private static final int POST_MESSAGE = 1;

    /**
     * Observer for MessageChannel events.
     */
    public static interface MessageChannelObserver {
        void onMessageChannelCreated();
    }

    private static class PostMessageFromWeb {
        public MessagePort port;
        public String message;

        public PostMessageFromWeb(MessagePort port, String message) {
            this.port = port;
            this.message = message;
        }
    }

    // The messages from JS to Java are posted to a message port on a background thread.
    // We do this to make any potential synchronization between Java and JS
    // easier for user programs.
    private static class MessageHandlerThread extends Thread {
        private static class MessageHandler extends Handler {
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

        private MessageHandler mHandler;

        public Handler getHandler() {
            return mHandler;
        }

        public void run() {
            Looper.prepare();
            mHandler = new MessageHandler();
            Looper.loop();
        }
    }

    // A thread safe storage for Message Ports.
    private static class MessagePortStorage {
        private SparseArray<MessagePort> mMessagePorts = new SparseArray<MessagePort>();
        private Object mLock = new Object();

        public void put(int portId, MessagePort m) {
            synchronized (mLock) {
                mMessagePorts.put(portId, m);
            }
        }
        public MessagePort get(int portId) {
            synchronized (mLock) {
                return mMessagePorts.get(portId);
            }
        }
    }

    private long mNativeMessagePortService;
    private MessagePortStorage mPortStorage = new MessagePortStorage();
    private MessageHandlerThread mMessageHandlerThread = new MessageHandlerThread();
    private ObserverList<MessageChannelObserver> mObserverList =
            new ObserverList<MessageChannelObserver>();

    AwMessagePortService() {
        mNativeMessagePortService = nativeInitAwMessagePortService();
        mMessageHandlerThread.start();
    }

    public void addObserver(MessageChannelObserver observer) {
        mObserverList.addObserver(observer);
    }

    public void removeObserver(MessageChannelObserver observer) {
        mObserverList.removeObserver(observer);
    }

    public void postMessage(int senderId, String message, int[] sentPorts) {
        // verify that webview still owns the port (not transferred)
        if (mPortStorage.get(senderId) == null) {
            throw new IllegalStateException("Cannot post to unknown port " + senderId);
        }
        removeSentPorts(sentPorts);
        if (mNativeMessagePortService == 0) return;
        nativePostAppToWebMessage(mNativeMessagePortService, senderId, message, sentPorts);
    }

    public void removeSentPorts(int[] sentPorts) {
        // verify that webview owns all the ports that are transferred
        if (sentPorts != null) {
            for (int port : sentPorts) {
                MessagePort p = mPortStorage.get(port);
                if (p == null) {
                    throw new IllegalStateException("Cannot transfer unknown port " + port);
                }
                mPortStorage.put(port, null);
            }
        }
    }

    public MessagePort[] createMessageChannel() {
        return new MessagePort[]{new MessagePort(this), new MessagePort(this)};
    }

    private MessagePort addPort(MessagePort m, int portId) {
        if (mPortStorage.get(portId) != null) {
            throw new IllegalStateException("Port already exists");
        }
        m.setPortId(portId);
        mPortStorage.put(portId, m);
        return m;
    }

    @CalledByNative
    private void onMessageChannelCreated(int portId1, int portId2,
            MessagePort[] ports) {
        ThreadUtils.assertOnUiThread();
        addPort(ports[0], portId1);
        addPort(ports[1], portId2);
        for (MessageChannelObserver observer : mObserverList) {
            observer.onMessageChannelCreated();
        }
    }

    @CalledByNative
    private void onPostMessage(int portId, String message, int[] ports) {
        PostMessageFromWeb m = new PostMessageFromWeb(mPortStorage.get(portId), message);
        Handler handler = mMessageHandlerThread.getHandler();
        if (handler == null) {
            // TODO(sgurun) handler could be null. But this logic will be removed.
            return;
        }
        Message msg = handler.obtainMessage(POST_MESSAGE, m);
        handler.sendMessage(msg);
    }

    @CalledByNative
    private void unregisterNativeAwMessagePortService() {
        mNativeMessagePortService = 0;
    }

    private native long nativeInitAwMessagePortService();
    private native void nativePostAppToWebMessage(long nativeAwMessagePortServiceImpl,
            int senderId, String message, int[] portIds);
}
