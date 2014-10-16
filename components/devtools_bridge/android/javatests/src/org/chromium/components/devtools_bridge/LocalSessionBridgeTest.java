// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;

import junit.framework.Assert;

import java.util.concurrent.Future;

/**
 * Tests for both client and server sessions bound with {@link LocalSessionBridge}.
 */
public class LocalSessionBridgeTest extends InstrumentationTestCase {
    private static final String SERVER_SOCKET_NAME =
            "org.chromium.components.devtools_bridge.LocalSessionBridgeTest.SERVER_SOCKET";
    private static final String CLIENT_SOCKET_NAME =
            "org.chromium.components.devtools_bridge.LocalSessionBridgeTest.CLIENT_SOCKET";

    private static final String REQUEST = "Request";
    private static final String RESPONSE = "Response";

    private LocalSessionBridge mBridge;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mBridge = new LocalSessionBridge(SERVER_SOCKET_NAME, CLIENT_SOCKET_NAME);
    }

    @Override
    public void tearDown() throws Exception {
        mBridge.dispose();
        super.tearDown();
    }

    @MediumTest
    public void testDisposeAfeterStart() {
        mBridge.start();
    }

    @MediumTest
    public void testNegotiating() throws InterruptedException {
        mBridge.start();
        mBridge.awaitNegotiated();
    }

    @MediumTest
    public void testOpenControlChannel() throws InterruptedException {
        mBridge.start();
        mBridge.awaitControlChannelOpened();
    }

    @MediumTest
    public void testClientAutocloseTimeout() throws InterruptedException {
        mBridge.setMessageDeliveryDelayMs(1000);
        mBridge.setClientAutoCloseTimeoutMs(100);
        mBridge.start();
        mBridge.awaitClientAutoClosed();
    }

    @MediumTest
    public void testServerAutocloseTimeout() throws InterruptedException {
        mBridge.setMessageDeliveryDelayMs(1000);
        mBridge.setServerAutoCloseTimeoutMs(100);
        mBridge.start();
        mBridge.awaitServerAutoClosed();
    }

    @MediumTest
    public void testRequestResponse() throws Exception {
        mBridge.start();

        LocalServerSocket serverListeningSocket = new LocalServerSocket(SERVER_SOCKET_NAME);
        Future<String> response = TestUtils.asyncRequest(CLIENT_SOCKET_NAME, REQUEST);
        LocalSocket serverSocket = serverListeningSocket.accept();
        String request = TestUtils.readAll(serverSocket);
        Assert.assertEquals(REQUEST, request);
        TestUtils.writeAndShutdown(serverSocket, RESPONSE);
        Assert.assertEquals(RESPONSE, response.get());
    }
}
