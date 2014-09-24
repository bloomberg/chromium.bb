// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.net.LocalSocket;
import android.net.LocalSocketAddress;

import java.io.IOException;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

/**
 * Utilities to testing a socket tunnel.
 */
public class TestUtils {
    private static final String CHARSET = "UTF-8";

    // Sends |request| string to UNIX socket socketName on another thread and returns
    // Future<String> for obtainings response.
    public static Future<String> asyncRequest(final String socketName, final String request) {

        final ExecutorService executor = Executors.newSingleThreadExecutor();

        return executor.submit(new Callable<String>() {
            @Override
            public String call() throws Exception {
                LocalSocket socket = new LocalSocket();
                socket.connect(new LocalSocketAddress(socketName));
                writeAndShutdown(socket, request);

                String response = readAll(socket);
                socket.close();

                executor.shutdown();
                return response;
            }
       });
    }

    public static void writeAndShutdown(LocalSocket socket, String data) throws IOException {
        socket.getOutputStream().write(data.getBytes(CHARSET));
        socket.getOutputStream().flush();
        socket.shutdownOutput();
    }

    // Reads all bytes from socket input stream until EOF and converts it to UTF-8 string.
    public static String readAll(LocalSocket socket) throws IOException {
        byte[] buffer = new byte[1000];
        int position = 0;
        while (true) {
            int count = socket.getInputStream().read(buffer, position, buffer.length - position);
            if (count == -1)
                break;
            position += count;
        }
        return new String(buffer, 0, position, CHARSET);
    }

    /**
     * Utility class for thread synchronization. Allow to track moving through series of steps.
     */
    public static class StateBarrier<T> {
        private T mState;
        private final Object mLock = new Object();

        public StateBarrier(T initialState) {
            mState = initialState;
        }

        // Waits until state |from| reached and change state to |to|.
        public void advance(T from, T to) {
            synchronized (mLock) {
                while (mState.equals(from)) {
                    try {
                        mLock.wait();
                    } catch (InterruptedException e) {
                        throw new RuntimeException(e);
                    }
                }
                mState = to;
                mLock.notifyAll();
            }
        }
    }

    /**
     * Helper with runs code on another thread and synchronously take the result.
     */
    public static class InvokeHelper<T> {
        private final CountDownLatch mDone = new CountDownLatch(1);

        private T mResult = null;
        private Exception mException = null;

        public void runOnTargetThread(Callable<T> callable) {
            try {
                mResult = callable.call();
            } catch (Exception e) {
                mException = e;
            }
            mDone.countDown();
        }

        public T takeResult() throws Exception {
            mDone.await();
            if (mException != null)
                throw mException;
            else
                return mResult;
        }
    }

    /**
     * Adapts Runnable to Callable<Void>.
     */
    public static class RunnableAdapter implements Callable<Void> {
        private final Runnable mAdaptee;

        public RunnableAdapter(Runnable adaptee) {
            mAdaptee = adaptee;
        }

        @Override
        public Void call() {
            mAdaptee.run();
            return null;
        }
    }
}
