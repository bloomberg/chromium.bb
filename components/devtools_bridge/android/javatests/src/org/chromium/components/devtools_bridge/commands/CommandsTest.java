// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.commands;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import java.util.HashMap;
import java.util.Map;

/**
 * Tests for {@link Commands}
 */
public class CommandsTest extends InstrumentationTestCase {
    @SmallTest
    public void testStartSessionCommand() throws Exception {
        CommandDefinition def = Command.Type.START_SESSION.definition;

        Assert.assertEquals("_startSession", def.shortName());
        Assert.assertEquals("base._startSession", def.fullName());

        Assert.assertEquals(3, getParamCount(def));
        Assert.assertEquals("_sessionId", getParam(def, 0).name());
        Assert.assertEquals("_config", getParam(def, 1).name());
        Assert.assertEquals("_offer", getParam(def, 2).name());

        Map<String, String> values = new HashMap<String, String>();
        values.put("_sessionId", "SESSION_ID");
        values.put("_config", "{}");
        values.put("_offer", "OFFER");

        Commands.StartSessionCommand command =
                (Commands.StartSessionCommand) def.newCommand("ID", values);

        Assert.assertEquals(Command.Type.START_SESSION, command.type);
        Assert.assertEquals("ID", command.id);
        Assert.assertEquals("SESSION_ID", command.sessionId);
        Assert.assertEquals("OFFER", command.offer);

        visitInParams(command, values);
    }

    @SmallTest
    public void testRenegotiateCommand() throws Exception {
        CommandDefinition def = Command.Type.RENEGOTIATE.definition;

        Assert.assertEquals("_renegotiate", def.shortName());
        Assert.assertEquals("base._renegotiate", def.fullName());

        Assert.assertEquals(2, getParamCount(def));
        Assert.assertEquals("_sessionId", getParam(def, 0).name());
        Assert.assertEquals("_offer", getParam(def, 1).name());

        Map<String, String> values = new HashMap<String, String>();
        values.put("_sessionId", "SESSION_ID");
        values.put("_offer", "OFFER");

        Commands.RenegotiateCommand command =
                (Commands.RenegotiateCommand) def.newCommand("ID", values);

        Assert.assertEquals(Command.Type.RENEGOTIATE, command.type);
        Assert.assertEquals("ID", command.id);
        Assert.assertEquals("OFFER", command.offer);

        visitInParams(command, values);
    }

    @SmallTest
    public void testIceExchangeCommand() throws Exception {
        CommandDefinition def = Command.Type.ICE_EXCHANGE.definition;

        Assert.assertEquals("_iceExchange", def.shortName());
        Assert.assertEquals("base._iceExchange", def.fullName());

        Assert.assertEquals(2, getParamCount(def));
        Assert.assertEquals("_sessionId", getParam(def, 0).name());
        Assert.assertEquals("_clientCandidates", getParam(def, 1).name());

        Map<String, String> values = new HashMap<String, String>();
        values.put("_sessionId", "SESSION_ID");
        values.put("_clientCandidates", "[\"candidate\"]");

        Commands.IceExchangeCommand command =
                (Commands.IceExchangeCommand) def.newCommand("ID", values);

        Assert.assertEquals(Command.Type.ICE_EXCHANGE, command.type);
        Assert.assertEquals("ID", command.id);

        visitInParams(command, values);
    }

    private void visitInParams(Command command, final Map<String, String> expectedValues) {
        final Map<String, String> actualValues = new HashMap<String, String>();
        command.visitInParams(new Command.ParamVisitor() {
            @Override
            public void visit(ParamDefinition param, String value) {
                String name = param.name();
                Assert.assertTrue(expectedValues.containsKey(name));
                actualValues.put(name, value);
            }
        });
        Assert.assertEquals(expectedValues.size(), actualValues.size());
    }

    private int getParamCount(CommandDefinition def) {
        int count = 0;
        for (ParamDefinition param : def.inParams()) {
            count++;
        }
        return count;
    }

    private ParamDefinition getParam(CommandDefinition def, int index) {
        for (ParamDefinition param : def.inParams()) {
            if (index == 0) return param;
            index--;
        }
        return null;
    }
}
