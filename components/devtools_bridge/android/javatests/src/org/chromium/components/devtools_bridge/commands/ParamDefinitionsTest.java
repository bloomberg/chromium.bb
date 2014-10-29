// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.commands;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.Assert;

import org.chromium.components.devtools_bridge.RTCConfiguration;

import java.util.List;

/**
 * Tests for {@link ParamDefinitions}
 */
public class ParamDefinitionsTest extends InstrumentationTestCase {
    @SmallTest
    public void testStringParam() throws Exception {
        ParamDefinition<String> param = ParamDefinitions.newStringParam("NAME");
        Assert.assertEquals("_NAME", param.name());
        Assert.assertEquals("string", param.type());
        Assert.assertEquals("STRING", param.fromString("STRING"));
        Assert.assertEquals("STRING", param.toString("STRING"));
    }

    @SmallTest
    public void testStringListParam() throws Exception {
        ParamDefinition<List<String>> param = ParamDefinitions.newStringListParam("NAME");
        Assert.assertEquals("_NAME", param.name());
        Assert.assertEquals("string", param.type());

        List<String> value = param.fromString("[\"ITEM1\",\"ITEM2\"]");
        Assert.assertEquals(2, value.size());
        Assert.assertEquals("ITEM1", value.get(0));
        Assert.assertEquals("ITEM2", value.get(1));

        try {
            param.fromString("{\"ITEM1\",\"ITEM2\"}");
            Assert.fail("Exception expected");
        } catch (CommandFormatException e) {
            // Expected.
        }

        List<String> clone = param.fromString(param.toString(value));
        Assert.assertEquals(value.size(), clone.size());
        Assert.assertEquals(value.get(0), clone.get(0));
        Assert.assertEquals(value.get(1), clone.get(1));
    }

    @SmallTest
    public void testConfigParam() throws Exception {
        ParamDefinition<RTCConfiguration> param = ParamDefinitions.newConfigParam("NAME");
        Assert.assertEquals("_NAME", param.name());
        Assert.assertEquals("string", param.type());

        RTCConfiguration value = param.fromString(
                "{\"iceServers\": [{\"uri\": \"http://example.org\"}]}");
        Assert.assertNotNull(value);
        Assert.assertEquals(1, value.iceServers.size());
        Assert.assertEquals("http://example.org", value.iceServers.get(0).uri);

        try {
            param.fromString("[");
            Assert.fail("Exception expected");
        } catch (CommandFormatException e) {
            // Expected.
        }

        value = new RTCConfiguration.Builder()
                .addIceServer("http://example.org/1", "USERNAME", "CREDENTIAL")
                .addIceServer("http://example.org/2", "USERNAME", "CREDENTIAL")
                .build();
        RTCConfiguration clone = param.fromString(param.toString(value));
        Assert.assertEquals(value.iceServers.size(), clone.iceServers.size());
        Assert.assertEquals(value.iceServers.get(0).username, clone.iceServers.get(0).username);
        Assert.assertEquals(value.iceServers.get(1).credential, clone.iceServers.get(1).credential);
    }
}
