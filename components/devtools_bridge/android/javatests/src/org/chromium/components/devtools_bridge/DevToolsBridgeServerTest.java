// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.content.Intent;
import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.os.IBinder;
import android.test.ServiceTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import java.util.concurrent.Callable;
import java.util.concurrent.Future;

/**
 * Tests for {@link DevToolsBridgeServer}
 */
public class DevToolsBridgeServerTest extends ServiceTestCase<LocalBindingTestService> {
    private static final String REQUEST = "Request";
    private static final String RESPONSE = "Response";
    private static final String SESSION_ID = "SESSION_ID";
    private static final String CLIENT_SOCKET_NAME =
            DevToolsBridgeServerTest.class.getName() + ".CLIENT_SOCKET_NAME";
    private SessionDependencyFactory mFactory;

    public DevToolsBridgeServerTest() {
        super(LocalBindingTestService.class);
    }

    private void invokeOnServiceThread(Runnable runnable) {
        getService().invokeOnServiceThread(runnable);
    }

    private <T> T invokeOnServiceThread(Callable<T> callable) throws Exception {
        return getService().invokeOnServiceThread(callable);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        setupService();
        mFactory = SessionDependencyFactory.newInstance();
    }

    @Override
    protected void tearDown() throws Exception {
        invokeOnServiceThread(new Runnable() {
            @Override
            public void run() {
                shutdownService();
            }
        });
        mFactory.dispose();
        super.tearDown();
    }

    @SmallTest
    public void testBind() throws Exception {
        Assert.assertNotNull(bindService().getReceiver());
    }

    @SmallTest
    public void testRequestResponse() throws Exception {
        LocalBindingTestService.LocalBinder binder = bindService();
        LocalServerSocket serverListeningSocket = new LocalServerSocket(binder.socketName());
        ClientSessionTestingHost clientSession = new ClientSessionTestingHost(
                mFactory, binder.getReceiver(), binder.createExecutor(),
                SESSION_ID, CLIENT_SOCKET_NAME);
        clientSession.start();

        Future<String> response = TestUtils.asyncRequest(CLIENT_SOCKET_NAME, REQUEST);
        LocalSocket serverSocket = serverListeningSocket.accept();
        String request = TestUtils.readAll(serverSocket);
        Assert.assertEquals(REQUEST, request);
        TestUtils.writeAndShutdown(serverSocket, RESPONSE);
        Assert.assertEquals(RESPONSE, response.get());

        clientSession.dispose();
    }

    private LocalBindingTestService.LocalBinder bindService() throws Exception {
        IBinder binder = invokeOnServiceThread(new Callable<IBinder>() {
            @Override
            public IBinder call() {
                return bindService(new Intent(getContext(), LocalBindingTestService.class));
            }
        });
        return (LocalBindingTestService.LocalBinder) binder;
    }
}
