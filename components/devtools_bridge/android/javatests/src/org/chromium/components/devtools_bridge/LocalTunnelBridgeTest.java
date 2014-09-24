// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import java.io.IOException;
import java.util.concurrent.Future;

/**
 * Tests for {@link SocketTunnelBridge}
 */
public class LocalTunnelBridgeTest extends InstrumentationTestCase {
    private static final String REQUEST = "Request";
    private static final String RESPONSE = "Response";

    private static final String SERVER_SOCKET_NAME =
            "org.chromium.components.devtools_bridge.LocalTunnelBridgeTest.SERVER_SOCKET";
    private static final String CLIENT_SOCKET_NAME =
            "org.chromium.components.devtools_bridge.LocalTunnelBridgeTest.CLIENT_SOCKET";

    private LocalTunnelBridge mBridge;
    private LocalServerSocket mServerSocket;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mServerSocket = new LocalServerSocket(SERVER_SOCKET_NAME);
    }

    private void startBridge() throws IOException {
        startBridge(SERVER_SOCKET_NAME);
    }

    private void startBridge(String serverSocketName) throws IOException {
        Assert.assertNull(mBridge);
        mBridge = new LocalTunnelBridge(serverSocketName, CLIENT_SOCKET_NAME);
        mBridge.start();
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();
        if (mBridge != null) {
            mBridge.dispose();
            mBridge = null;
        }
        mServerSocket.close();
    }

    @SmallTest
    public void testStartStop() throws Exception {
        startBridge();
        mBridge.stop();
    }

    @SmallTest
    public void testRequestResponse() throws Exception {
        startBridge();

        Future<String> response = TestUtils.asyncRequest(CLIENT_SOCKET_NAME, REQUEST);

        LocalSocket socket = mServerSocket.accept();
        String request = TestUtils.readAll(socket);
        TestUtils.writeAndShutdown(socket, RESPONSE);

        Assert.assertEquals(REQUEST, request);

        Assert.assertEquals(RESPONSE, response.get());
        socket.close();

        mBridge.stop();
    }

    @SmallTest
    public void testRequestFailure1() throws Exception {
        startBridge();

        Future<String> response = TestUtils.asyncRequest(CLIENT_SOCKET_NAME, REQUEST);
        LocalSocket socket = mServerSocket.accept();
        int firstByte = socket.getInputStream().read();

        Assert.assertEquals((int) REQUEST.charAt(0), firstByte);

        socket.close();

        Assert.assertEquals("", response.get());

        mBridge.waitAllConnectionsClosed();
        mBridge.stop();
    }

    @SmallTest
    public void testRequestFailure2() throws Exception {
        startBridge("jdwp-control"); // Android system socket will reject connection.

        Future<String> response = TestUtils.asyncRequest(CLIENT_SOCKET_NAME, REQUEST);

        Assert.assertEquals("", response.get());

        mBridge.waitAllConnectionsClosed();
        mBridge.stop();
    }
}
