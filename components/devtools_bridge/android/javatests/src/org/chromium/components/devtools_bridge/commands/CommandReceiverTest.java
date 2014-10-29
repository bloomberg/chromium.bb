// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.commands;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import org.chromium.components.devtools_bridge.RTCConfiguration;
import org.chromium.components.devtools_bridge.SignalingReceiverMock;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link CommandReceiver}
 */
public class CommandReceiverTest extends InstrumentationTestCase {
    private static final RTCConfiguration CONFIG = new RTCConfiguration.Builder()
            .build();

    private final SignalingReceiverMock mMock = new SignalingReceiverMock();
    private final CommandReceiver mReceiver = new CommandReceiver(mMock);
    private final CompletionHandler mCompletionHandler = new CompletionHandler();

    private static class CompletionHandler implements Runnable {
        public boolean done = false;

        @Override
        public void run() {
            done = true;
        }
    };

    @SmallTest
    public void testStartSessionCommand() {
        Commands.StartSessionCommand command =
                new Commands.StartSessionCommand("SESSION_ID", CONFIG, "OFFER");

        mReceiver.receive(command, mCompletionHandler);
        Assert.assertEquals("SESSION_ID", mMock.sessionId);
        Assert.assertEquals("OFFER", mMock.offer);

        mMock.negotiationCallback.onSuccess("ANSWER");
        Assert.assertTrue(mCompletionHandler.done);
        Assert.assertEquals(command.getResult(), "ANSWER");
    }

    @SmallTest
    public void testRenegotiateCommand() {
        Commands.RenegotiateCommand command =
                new Commands.RenegotiateCommand("SESSION_ID", "OFFER");

        mReceiver.receive(command, mCompletionHandler);
        Assert.assertEquals("SESSION_ID", mMock.sessionId);
        Assert.assertEquals("OFFER", mMock.offer);

        mMock.negotiationCallback.onSuccess("ANSWER");
        Assert.assertTrue(mCompletionHandler.done);
        Assert.assertEquals(command.getResult(), "ANSWER");
    }

    @SmallTest
    public void testIceExchangeCommand() {
        List<String> candidates = new ArrayList<String>();

        Commands.IceExchangeCommand command =
                new Commands.IceExchangeCommand("SESSION_ID", candidates);

        mReceiver.receive(command, mCompletionHandler);
        Assert.assertEquals("SESSION_ID", mMock.sessionId);

        mMock.iceExchangeCallback.onSuccess(candidates);
        Assert.assertTrue(mCompletionHandler.done);
    }

    @SmallTest
    public void testCommandFailure() {
        Commands.RenegotiateCommand command =
                new Commands.RenegotiateCommand("SESSION_ID", "OFFER");

        mReceiver.receive(command, mCompletionHandler);

        mMock.negotiationCallback.onFailure("ERROR_MESSAGE");
        Assert.assertTrue(mCompletionHandler.done);
        Assert.assertEquals(command.getErrorMessage(), "ERROR_MESSAGE");
    }
}
