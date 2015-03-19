// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

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

    /**
     * Observer for MessageChannel events.
     */
    public static interface MessageChannelObserver {
        void onMessageChannelCreated();
    }

    // A thread safe storage for Message Ports.
    private static class MessagePortStorage {
        private SparseArray<AwMessagePort> mMessagePorts = new SparseArray<AwMessagePort>();
        private Object mLock = new Object();

        public void remove(int portId) {
            synchronized (mLock) {
                mMessagePorts.remove(portId);
            }
        }

        public void put(int portId, AwMessagePort m) {
            synchronized (mLock) {
                mMessagePorts.put(portId, m);
            }
        }
        public AwMessagePort get(int portId) {
            synchronized (mLock) {
                return mMessagePorts.get(portId);
            }
        }
    }

    private long mNativeMessagePortService;
    private MessagePortStorage mPortStorage = new MessagePortStorage();
    private ObserverList<MessageChannelObserver> mObserverList =
            new ObserverList<MessageChannelObserver>();

    AwMessagePortService() {
        mNativeMessagePortService = nativeInitAwMessagePortService();
    }

    public void addObserver(MessageChannelObserver observer) {
        mObserverList.addObserver(observer);
    }

    public void removeObserver(MessageChannelObserver observer) {
        mObserverList.removeObserver(observer);
    }

    public void closePort(int messagePortId) {
        mPortStorage.remove(messagePortId);
        if (mNativeMessagePortService == 0) return;
        nativeClosePort(mNativeMessagePortService, messagePortId);
    }

    public void postMessage(int senderId, String message, int[] sentPorts) {
        // verify that webview still owns the port (not transferred)
        if (mPortStorage.get(senderId) == null) {
            throw new IllegalStateException("Cannot post to unknown port " + senderId);
        }
        if (mNativeMessagePortService == 0) return;
        nativePostAppToWebMessage(mNativeMessagePortService, senderId, message, sentPorts);
    }

    public void removeSentPorts(int[] sentPorts) {
        // verify that webview owns all the ports that are transferred
        if (sentPorts != null) {
            for (int port : sentPorts) {
                AwMessagePort p = mPortStorage.get(port);
                if (p == null) {
                    throw new IllegalStateException("Cannot transfer unknown port " + port);
                }
                mPortStorage.remove(port);
            }
        }
    }

    public AwMessagePort[] createMessageChannel() {
        return new AwMessagePort[]{new AwMessagePort(this), new AwMessagePort(this)};
    }

    // Called on UI thread.
    public void releaseMessages(int portId) {
        if (mNativeMessagePortService == 0) return;
        nativeReleaseMessages(mNativeMessagePortService, portId);
    }

    private AwMessagePort addPort(AwMessagePort m, int portId) {
        if (mPortStorage.get(portId) != null) {
            throw new IllegalStateException("Port already exists");
        }
        m.setPortId(portId);
        mPortStorage.put(portId, m);
        return m;
    }

    @CalledByNative
    private void onMessageChannelCreated(int portId1, int portId2,
            AwMessagePort[] ports) {
        ThreadUtils.assertOnUiThread();
        addPort(ports[0], portId1);
        addPort(ports[1], portId2);
        for (MessageChannelObserver observer : mObserverList) {
            observer.onMessageChannelCreated();
        }
    }

    // Called on IO thread.
    @CalledByNative
    private void onReceivedMessage(int portId, String message, int[] ports) {
        AwMessagePort[] messagePorts = null;
        for (int i = 0; i < ports.length; i++) {
            if (messagePorts == null) {
                messagePorts = new AwMessagePort[ports.length];
            }
            messagePorts[i] = addPort(new AwMessagePort(this), ports[i]);
        }
        mPortStorage.get(portId).onReceivedMessage(message, messagePorts);
    }

    @CalledByNative
    private void unregisterNativeAwMessagePortService() {
        mNativeMessagePortService = 0;
    }

    private native long nativeInitAwMessagePortService();
    private native void nativePostAppToWebMessage(long nativeAwMessagePortServiceImpl,
            int senderId, String message, int[] portIds);
    private native void nativeClosePort(long nativeAwMessagePortServiceImpl,
            int messagePortId);
    private native void nativeReleaseMessages(long nativeAwMessagePortServiceImpl,
            int messagePortId);
}
