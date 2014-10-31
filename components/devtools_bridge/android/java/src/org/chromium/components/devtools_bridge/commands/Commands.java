// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.commands;

import org.chromium.components.devtools_bridge.RTCConfiguration;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * Implementation of all commands.
 */
final class Commands {
    public static final String NO_ID = null;

    // In params.
    private static final ParamDefinition<String> PARAM_SESSION_ID =
            ParamDefinitions.newStringParam("sessionId");
    private static final ParamDefinition<RTCConfiguration> PARAM_CONFIG =
            ParamDefinitions.newConfigParam("config");
    private static final ParamDefinition<String> PARAM_OFFER =
            ParamDefinitions.newStringParam("offer");
    private static final ParamDefinition<List<String>> PARAM_CLIENT_CANDIDATES =
            ParamDefinitions.newStringListParam("clientCandidates");

    // Out params.
    private static final ParamDefinition<String> PARAM_ANSWER =
            ParamDefinitions.newStringParam("answer");
    private static final ParamDefinition<List<String>> PARAM_SERVER_CANDIDATES =
            ParamDefinitions.newStringListParam("serverCandidates");

    /**
     * Common base class for signaling commands. All commands needed so far have a session ID
     * and a single out parameter (result).
     */
    abstract static class SignalingCommandBase<R> extends Command {
        public final String sessionId;
        public R mResult;

        protected SignalingCommandBase(Type type, String id, String sessionId) {
            super(type, id);
            this.sessionId = sessionId;
        }

        @Override
        public void visitInParams(ParamVisitor visitor) {
            PARAM_SESSION_ID.pass(visitor, sessionId);
        }

        @Override
        public final void visitOutParams(ParamVisitor visitor) {
            resultDefinition().pass(visitor, mResult);
        }

        @Override
        protected void setOutParams(Map<String, String> actualOutParams)
                throws CommandFormatException {
            mResult = resultDefinition().checkAndGet(actualOutParams);
        }

        public final void setResult(R result) {
            mResult = result;
            setDone();
        }

        public final R getResult() {
            assert state() == State.DONE;
            return mResult;
        }

        protected abstract ParamDefinition<R> resultDefinition();
    }

    static final class StartSessionCommand extends SignalingCommandBase<String> {
        public final RTCConfiguration config;
        public final String offer;

        public static final CommandDefinition DEFINITION = new CommandDefinition(
                "startSession", params(PARAM_SESSION_ID, PARAM_CONFIG, PARAM_OFFER)) {
            @Override
            public Command newCommand(String id, Map<String, String> actualParameters)
                    throws CommandFormatException {
                return new StartSessionCommand(
                        id,
                        PARAM_SESSION_ID.get(actualParameters),
                        PARAM_CONFIG.get(actualParameters),
                        PARAM_OFFER.get(actualParameters));
            }
        };

        private StartSessionCommand(
                String id, String sessionId, RTCConfiguration config, String offer) {
            super(Type.START_SESSION, id, sessionId);
            this.config = config;
            this.offer = offer;
        }

        public StartSessionCommand(String sessionId, RTCConfiguration config, String offer) {
            this(NO_ID, sessionId, config, offer);
        }

        @Override
        public void visitInParams(ParamVisitor visitor) {
            super.visitInParams(visitor);
            PARAM_CONFIG.pass(visitor, config);
            PARAM_OFFER.pass(visitor, offer);
        }

        @Override
        protected ParamDefinition<String> resultDefinition() {
            return PARAM_ANSWER;
        }
    }

    static final class IceExchangeCommand extends SignalingCommandBase<List<String>> {
        private static final String SERVER_CANDIDATES = "serverCandidates";

        public final List<String> clientCandidates;

        public static final CommandDefinition DEFINITION = new CommandDefinition(
                "iceExchange", params(PARAM_SESSION_ID, PARAM_CLIENT_CANDIDATES)) {
            @Override
            public Command newCommand(String id, Map<String, String> actualParameters)
                    throws CommandFormatException {
                return new IceExchangeCommand(
                        id,
                        PARAM_SESSION_ID.get(actualParameters),
                        PARAM_CLIENT_CANDIDATES.get(actualParameters));
            }
        };

        private IceExchangeCommand(String id, String sessionId, List<String> clientCandidates) {
            super(Type.ICE_EXCHANGE, id, sessionId);
            this.clientCandidates = clientCandidates;
        }

        public IceExchangeCommand(String sessionId, List<String> clientCandidates) {
            this(NO_ID, sessionId, clientCandidates);
        }

        @Override
        public void visitInParams(ParamVisitor visitor) {
            super.visitInParams(visitor);
            PARAM_CLIENT_CANDIDATES.pass(visitor, clientCandidates);
        }

        @Override
        protected ParamDefinition<List<String>> resultDefinition() {
            return PARAM_SERVER_CANDIDATES;
        }
    }

    static final class RenegotiateCommand extends SignalingCommandBase<String> {
        public final String offer;

        public static final CommandDefinition DEFINITION = new CommandDefinition(
                "renegotiate", params(PARAM_SESSION_ID, PARAM_OFFER)) {
            @Override
            public Command newCommand(String id, Map<String, String> actualParameters)
                    throws CommandFormatException {
                return new RenegotiateCommand(
                        id,
                        PARAM_SESSION_ID.get(actualParameters),
                        PARAM_OFFER.get(actualParameters));
            }
        };

        private RenegotiateCommand(String id, String sessionId, String offer) {
            super(Type.RENEGOTIATE, id, sessionId);
            this.offer = offer;
        }

        public RenegotiateCommand(String sessionId, String offer) {
            this(NO_ID, sessionId, offer);
        }

        @Override
        public void visitInParams(ParamVisitor visitor) {
            super.visitInParams(visitor);
            PARAM_OFFER.pass(visitor, offer);
        }

        @Override
        protected ParamDefinition<String> resultDefinition() {
            return PARAM_ANSWER;
        }
    }

    private static List<ParamDefinition<?>> params(ParamDefinition<?>... values) {
        return Collections.unmodifiableList(Arrays.asList(values));
    }
}
