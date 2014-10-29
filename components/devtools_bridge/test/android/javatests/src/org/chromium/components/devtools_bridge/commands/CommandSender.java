// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.commands;

import org.chromium.components.devtools_bridge.RTCConfiguration;
import org.chromium.components.devtools_bridge.SessionBase;
import org.chromium.components.devtools_bridge.SignalingReceiver;

import java.util.List;

/**
 * Implementation of SignalingReceiver which wraps each call into a Command object.
 */
public abstract class CommandSender implements SignalingReceiver {
    protected abstract void send(Command command, Runnable completionCallback);

    @Override
    public void startSession(
            String sessionId, RTCConfiguration config, String offer,
            final SessionBase.NegotiationCallback callback) {
        final Commands.StartSessionCommand command =
                new Commands.StartSessionCommand(sessionId, config, offer);
        send(command, new Runnable() {
            @Override
            public void run() {
                if (!handleFailure(command, callback)) {
                    callback.onSuccess(command.getResult());
                }
            }
        });
    }

    @Override
    public void renegotiate(
            String sessionId, String offer, final SessionBase.NegotiationCallback callback) {
        final Commands.RenegotiateCommand command =
                new Commands.RenegotiateCommand(sessionId, offer);
        send(command, new Runnable() {
            @Override
            public void run() {
                if (!handleFailure(command, callback)) {
                    callback.onSuccess(command.getResult());
                }
            }
        });
    }

    @Override
    public void iceExchange(
            String sessionId, List<String> clientCandidates,
            final SessionBase.IceExchangeCallback callback) {
        final Commands.IceExchangeCommand command =
                new Commands.IceExchangeCommand(sessionId, clientCandidates);
        send(command, new Runnable() {
            @Override
            public void run() {
                if (!handleFailure(command, callback)) {
                    callback.onSuccess(command.getResult());
                }
            }
        });
    }

    private static boolean handleFailure(Command command, SessionBase.ServerCallback callback) {
        if (command.state() == Command.State.DONE) return false;
        if (command.state() == Command.State.ERROR) {
            callback.onFailure(command.getErrorMessage());
        } else {
            callback.onFailure("Invalid command state: " + command.state());
        }
        return true;
    }
}
