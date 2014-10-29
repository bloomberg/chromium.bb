// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.commands;

import org.chromium.components.devtools_bridge.SessionBase;
import org.chromium.components.devtools_bridge.SignalingReceiver;

import java.util.List;

/**
 * Converts commands to SignalingReceiver's calls.
 */
public class CommandReceiver {
    private final SignalingReceiver mBase;

    public CommandReceiver(SignalingReceiver base) {
        mBase = base;
    }

    public void receive(Command command, Runnable completionHandler) {
        switch (command.type) {
            case START_SESSION:
                onCommand((Commands.StartSessionCommand) command, completionHandler);
                break;

            case RENEGOTIATE:
                onCommand((Commands.RenegotiateCommand) command, completionHandler);
                break;

            case ICE_EXCHANGE:
                onCommand((Commands.IceExchangeCommand) command, completionHandler);
                break;

            default:
                assert false;
        }
    }

    private void onCommand(
            final Commands.StartSessionCommand command, final Runnable completionHandler) {
        mBase.startSession(
                command.sessionId, command.config, command.offer,
                new SessionBase.NegotiationCallback() {
                    @Override
                    public void onSuccess(String answer) {
                        command.setResult(answer);
                        completionHandler.run();
                    }

                    @Override
                    public void onFailure(String errorMessage) {
                        command.setFailure(errorMessage);
                        completionHandler.run();
                    }
                });
    }

    private void onCommand(
            final Commands.RenegotiateCommand command, final Runnable completionHandler) {
        mBase.renegotiate(
                command.sessionId, command.offer,
                new SessionBase.NegotiationCallback() {
                    @Override
                    public void onSuccess(String answer) {
                        command.setResult(answer);
                        completionHandler.run();
                    }

                    @Override
                    public void onFailure(String errorMessage) {
                        command.setFailure(errorMessage);
                        completionHandler.run();
                    }
                });
    }

    private void onCommand(
            final Commands.IceExchangeCommand command, final Runnable completionHandler) {
        mBase.iceExchange(
                command.sessionId, command.clientCandidates,
                new SessionBase.IceExchangeCallback() {
                    @Override
                    public void onSuccess(List<String> serverCandidates) {
                        command.setResult(serverCandidates);
                        completionHandler.run();
                    }

                    @Override
                    public void onFailure(String errorMessage) {
                        command.setFailure(errorMessage);
                        completionHandler.run();
                    }
                });
    }
}
