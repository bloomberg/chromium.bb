// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.util.SparseArray;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.MessagePortService;

/**
 * Provides the Message Channel functionality for Android Apps
 * (including WebView embedders and Chrome Custom Tabs clients). Specifically
 * manages the message ports that are associated with a message channel and
 * handles posting/receiving messages to/from them.
 * See https://html.spec.whatwg.org/multipage/comms.html#messagechannel for
 * further information on message channels.
 *
 * The message ports have unique IDs. In Android implementation,
 * the message ports are only known by their IDs at the native side.
 * At the java side, the embedder deals with MessagePort objects. The mapping
 * from an ID to an object is in AppWebMessagePortService. AppWebMessagePortService
 * keeps a strong ref to MessagePort objects until they are closed.
 *
 * Ownership: The Java AppWebMessagePortService has to be owned by Java side global.
 * The native AppWebMessagePortService is a singleton. The
 * native peer maintains a weak ref to the java object and deregisters itself
 * before being deleted.
 *
 * All methods are called on UI thread except as noted.
 */
@JNINamespace("content")
public class AppWebMessagePortService implements MessagePortService {
    private static final String TAG = "AppWebMessagePortService";

    /**
     * Observer for MessageChannel events.
     */
    public static interface MessageChannelObserver { void onMessageChannelCreated(); }

    // A thread safe storage for Message Ports.
    private static class MessagePortStorage {
        private SparseArray<AppWebMessagePort> mMessagePorts = new SparseArray<AppWebMessagePort>();
        private final Object mLock = new Object();

        public void remove(int portId) {
            synchronized (mLock) {
                mMessagePorts.remove(portId);
            }
        }

        public void put(int portId, AppWebMessagePort m) {
            synchronized (mLock) {
                mMessagePorts.put(portId, m);
            }
        }
        public AppWebMessagePort get(int portId) {
            synchronized (mLock) {
                return mMessagePorts.get(portId);
            }
        }
    }

    private long mNativeMessagePortService;
    private MessagePortStorage mPortStorage = new MessagePortStorage();
    private ObserverList<MessageChannelObserver> mObserverList =
            new ObserverList<MessageChannelObserver>();

    public AppWebMessagePortService() {
        mNativeMessagePortService = nativeInitAppWebMessagePortService();
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
        // verify that the port is owned by service still (not transferred).
        if (mPortStorage.get(senderId) == null) {
            throw new IllegalStateException("Cannot post to unknown port " + senderId);
        }
        if (mNativeMessagePortService == 0) return;
        nativePostAppToWebMessage(mNativeMessagePortService, senderId, message, sentPorts);
    }

    public void removeSentPorts(int[] sentPorts) {
        // verify that this service still owns all the ports that are transferred
        if (sentPorts != null) {
            for (int port : sentPorts) {
                AppWebMessagePort p = mPortStorage.get(port);
                if (p == null) {
                    throw new IllegalStateException("Cannot transfer unknown port " + port);
                }
                mPortStorage.remove(port);
            }
        }
    }

    @Override
    public AppWebMessagePort[] createMessageChannel() {
        return new AppWebMessagePort[] {new AppWebMessagePort(this), new AppWebMessagePort(this)};
    }

    // Called on UI thread.
    public void releaseMessages(int portId) {
        if (mNativeMessagePortService == 0) return;
        nativeReleaseMessages(mNativeMessagePortService, portId);
    }

    private AppWebMessagePort addPort(AppWebMessagePort m, int portId) {
        if (mPortStorage.get(portId) != null) {
            throw new IllegalStateException("Port already exists");
        }
        m.setPortId(portId);
        mPortStorage.put(portId, m);
        return m;
    }

    @CalledByNative
    private void onMessageChannelCreated(int portId1, int portId2, AppWebMessagePort[] ports) {
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
        AppWebMessagePort[] messagePorts = null;
        for (int i = 0; i < ports.length; i++) {
            if (messagePorts == null) {
                messagePorts = new AppWebMessagePort[ports.length];
            }
            messagePorts[i] = addPort(new AppWebMessagePort(this), ports[i]);
        }
        mPortStorage.get(portId).onReceivedMessage(message, messagePorts);
    }

    @CalledByNative
    private void unregisterNativeAppWebMessagePortService() {
        mNativeMessagePortService = 0;
    }

    private native long nativeInitAppWebMessagePortService();
    private native void nativePostAppToWebMessage(
            long nativeAppWebMessagePortServiceImpl, int senderId, String message, int[] portIds);
    private native void nativeClosePort(long nativeAppWebMessagePortServiceImpl, int messagePortId);
    private native void nativeReleaseMessages(
            long nativeAppWebMessagePortServiceImpl, int messagePortId);
}
