// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.commands;

import java.util.Map;

/**
 * Base class for a command. Command is an abstracton over GCD's command. Command
 * has state, in- and out-parameters. Both parameters are encoded as a hash of strings.
 */
public abstract class Command {
    public final Type type;
    public final String id;

    private State mState = State.INITIAL;
    private String mErrorMessage;

    public enum State {
        INITIAL, DONE, ERROR
    }

    public enum Type {
        START_SESSION(Commands.StartSessionCommand.DEFINITION),
        ICE_EXCHANGE(Commands.IceExchangeCommand.DEFINITION),
        RENEGOTIATE(Commands.RenegotiateCommand.DEFINITION);

        public final CommandDefinition definition;

        Type(CommandDefinition definition) {
            this.definition = definition;
        }
    }

    /**
     * Provides access to parameters values with the Visitor pattern.
     */
    public interface ParamVisitor {
        void visit(ParamDefinition<?> param, String value);
    }

    protected Command(Type type, String id) {
        assert type != null;

        this.type = type;
        this.id = id;
    }

    public State state() {
        return mState;
    }

    public abstract void visitInParams(ParamVisitor visitor);

    public abstract void visitOutParams(ParamVisitor visitor);

    protected abstract void setOutParams(Map<String, String> actualOutParams)
            throws CommandFormatException;

    protected final void setDone() {
        assert mState == State.INITIAL;

        mState = State.DONE;
    }

    public void setSuccess(Map<String, String> actualOutParams) throws CommandFormatException {
        setOutParams(actualOutParams);
        setDone();
    }

    public void setFailure(String errorMessage) {
        assert mState == State.INITIAL;

        mState = State.ERROR;
        mErrorMessage = errorMessage;
    }

    public String getErrorMessage() {
        assert mState == State.ERROR;

        return mErrorMessage;
    }
}
