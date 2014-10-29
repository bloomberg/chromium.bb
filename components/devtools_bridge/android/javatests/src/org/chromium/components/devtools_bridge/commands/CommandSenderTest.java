// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.commands;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import org.chromium.components.devtools_bridge.RTCConfiguration;
import org.chromium.components.devtools_bridge.SessionBase;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link CommandSender}
 */
public class CommandSenderTest extends InstrumentationTestCase {
    private static final RTCConfiguration CONFIG = new RTCConfiguration.Builder()
            .addIceServer("http://example.org")
            .build();

    private Command mCommand;
    private Runnable mCompletionCallback;
    private String mAnswer;
    private String mErrorMessage;
    private List<String> mServerCandidates;

    @SmallTest
    public void testStartSessionCommand() {
        new SenderMock().startSession("SESSION_ID", CONFIG, "OFFER", new NegotiationCallbackMock());

        Commands.StartSessionCommand command = (Commands.StartSessionCommand) mCommand;
        Assert.assertNotNull(command);

        Assert.assertEquals("SESSION_ID", command.sessionId);
        Assert.assertEquals("OFFER", command.offer);

        command.setResult("ANSWER");

        mCompletionCallback.run();
        Assert.assertEquals("ANSWER", mAnswer);
        Assert.assertNull(mErrorMessage);
    }

    @SmallTest
    public void testRenegotiateCommand() {
        new SenderMock().renegotiate("SESSION_ID", "OFFER", new NegotiationCallbackMock());
        Commands.RenegotiateCommand command = (Commands.RenegotiateCommand) mCommand;
        Assert.assertNotNull(command);

        Assert.assertEquals("SESSION_ID", command.sessionId);
        Assert.assertEquals("OFFER", command.offer);

        command.setResult("ANSWER");

        mCompletionCallback.run();
        Assert.assertEquals("ANSWER", mAnswer);
        Assert.assertNull(mErrorMessage);
    }

    @SmallTest
    public void testIceExchangeCommand() {
        ArrayList<String> candidates = new ArrayList<String>();
        new SenderMock().iceExchange("SESSION_ID", candidates, new IceExchangeCallbackMock());
        Commands.IceExchangeCommand command = (Commands.IceExchangeCommand) mCommand;
        Assert.assertNotNull(command);

        Assert.assertEquals("SESSION_ID", command.sessionId);
        command.setResult(candidates);

        mCompletionCallback.run();
        Assert.assertNull(mErrorMessage);
    }

    @SmallTest
    public void testCommandFailure() {
        new SenderMock().renegotiate("SESSION_ID", "OFFER", new NegotiationCallbackMock());

        Commands.RenegotiateCommand command = (Commands.RenegotiateCommand) mCommand;
        Assert.assertNotNull(command);

        command.setFailure("TEST_MESSAGE");

        mCompletionCallback.run();
        Assert.assertNull(mAnswer);
        Assert.assertEquals("TEST_MESSAGE", mErrorMessage);
    }

    private class SenderMock extends CommandSender {
        @Override
        protected void send(Command command, Runnable completionCallback) {
            mCommand = command;
            mCompletionCallback = completionCallback;
        }
    }

    private class NegotiationCallbackMock implements SessionBase.NegotiationCallback {
        @Override
        public void onSuccess(String answer) {
            mAnswer = answer;
        }

        @Override
        public void onFailure(String errorMessage) {
            mErrorMessage = errorMessage;
        }
    }

    private class IceExchangeCallbackMock implements SessionBase.IceExchangeCallback {
        @Override
        public void onSuccess(List<String> serverCandidates) {
            mServerCandidates = serverCandidates;
        }

        @Override
        public void onFailure(String errorMessage) {
            mErrorMessage = errorMessage;
        }
    }
}
