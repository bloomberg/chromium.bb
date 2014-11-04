// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import org.chromium.components.devtools_bridge.commands.Command;
import org.chromium.components.devtools_bridge.commands.CommandReceiver;
import org.chromium.components.devtools_bridge.commands.CommandSender;

/**
 * Helper proxy that binds client and server sessions living on different executors.
 */
final class SignalingReceiverProxy extends CommandSender {
    private final CommandReceiver mReceiver;
    private final SessionBase.Executor mServerExecutor;
    private final SessionBase.Executor mClientExecutor;
    private final int mDelayMs;

    public SignalingReceiverProxy(
            SessionBase.Executor serverExecutor,
            SessionBase.Executor clientExecutor,
            SignalingReceiver server,
            int delayMs) {
        mServerExecutor = serverExecutor;
        mClientExecutor = clientExecutor;
        mReceiver = new CommandReceiver(server);
        mDelayMs = delayMs;
    }

    public SessionBase.Executor serverExecutor() {
        return mServerExecutor;
    }

    public SessionBase.Executor clientExecutor() {
        return mClientExecutor;
    }

    @Override
    protected void send(final Command command, final Runnable completionCallback) {
        assert mClientExecutor.isCalledOnSessionThread();

        mServerExecutor.postOnSessionThread(mDelayMs, new Runnable() {
                    @Override
                    public void run() {
                        mReceiver.receive(command, new Runnable() {
                            @Override
                            public void run() {
                                assert mServerExecutor.isCalledOnSessionThread();
                                mClientExecutor.postOnSessionThread(mDelayMs, completionCallback);
                            }
                        });
                    }
                });
    }
}
