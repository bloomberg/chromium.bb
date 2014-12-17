// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.content.Context;
import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import org.chromium.components.devtools_bridge.util.LooperExecutor;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Future;

/**
 * Tests for {@link DevToolsBridgeServer}
 */
public class DevToolsBridgeServerTest extends InstrumentationTestCase {
    private static final String REQUEST = "Request";
    private static final String RESPONSE = "Response";
    private static final String SESSION_ID = "SESSION_ID";
    private static final String CLIENT_SOCKET_NAME =
            DevToolsBridgeServerTest.class.getName() + ".CLIENT_SOCKET_NAME";
    private static final String SERVER_SOCKET_NAME =
            DevToolsBridgeServerTest.class.getName() + ".SERVER_SOCKET_NAME";
    private SessionDependencyFactory mFactory;

    private LooperExecutor mServerExecutor;
    private DevToolsBridgeServer mServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mFactory = SessionDependencyFactory.newInstance();
        mServer = new DevToolsBridgeServer(new ServerDelegate());
        mServerExecutor = LooperExecutor.newInstanceForMainLooper(
                getInstrumentation().getTargetContext());
    }

    @Override
    protected void tearDown() throws Exception {
        final CountDownLatch done = new CountDownLatch(1);
        mServerExecutor.postOnSessionThread(0, new Runnable() {
            @Override
            public void run() {
                mServer.dispose();
                mServer = null;
                done.countDown();
            }
        });
        done.await();
        mFactory.dispose();
        super.tearDown();
    }

    @SmallTest
    public void testRequestResponse() throws Exception {
        LocalServerSocket serverListeningSocket = new LocalServerSocket(SERVER_SOCKET_NAME);
        ClientSessionTestingHost clientSession = new ClientSessionTestingHost(
                mFactory, mServer, mServerExecutor, SESSION_ID, CLIENT_SOCKET_NAME);
        clientSession.start();

        Future<String> response = TestUtils.asyncRequest(CLIENT_SOCKET_NAME, REQUEST);
        LocalSocket serverSocket = serverListeningSocket.accept();
        String request = TestUtils.read(serverSocket, REQUEST.length());
        Assert.assertEquals(REQUEST, request);
        TestUtils.write(serverSocket, RESPONSE);
        serverSocket.close();
        Assert.assertEquals(RESPONSE, response.get());

        clientSession.dispose();
    }

    private class ServerDelegate implements DevToolsBridgeServer.Delegate {
        @Override
        public Context getContext() {
            return getInstrumentation().getTargetContext();
        }

        @Override
        public void querySocketName(DevToolsBridgeServer.QuerySocketCallback callback) {
            callback.onSuccess(SERVER_SOCKET_NAME);
        }

        @Override
        public void onSessionCountChange(int count) {}
    }
}
