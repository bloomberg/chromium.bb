// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.gcd;

import android.util.JsonWriter;

import org.chromium.components.devtools_bridge.commands.Command;
import org.chromium.components.devtools_bridge.commands.ParamDefinition;

import java.io.IOException;
import java.io.StringWriter;

/**
 * Helper class for constructing GCD JSON messages (HTTP requests) used in the DevTools bridge.
 */
public final class TestMessageWriter {
    private final StringWriter mStringWriter;
    private final JsonWriter mWriter;
    boolean mClosed = false;

    public TestMessageWriter() {
        mStringWriter = new StringWriter();
        mWriter = new JsonWriter(mStringWriter);
    }

    public TestMessageWriter close() throws IOException {
        assert !mClosed;
        mWriter.close();
        mClosed = true;
        return this;
    }

    @Override
    public String toString() {
        assert mClosed;
        return mStringWriter.toString();
    }

    public TestMessageWriter writeCommand(
            String remoteInstanceId, Command command) throws IOException {
        mWriter.beginObject()
                .name("deviceId").value(remoteInstanceId)
                .name("name").value(command.type.definition.fullName());

        mWriter.name("parameters").beginObject();
        command.visitInParams(new Command.ParamVisitor() {
            @Override
            public void visit(ParamDefinition<?> param, String value) {
                try {
                    mWriter.name(param.name()).value(value);
                } catch (IOException e) {
                    // IO excepion must not happen since we writng to a string.
                    throw new RuntimeException(e);
                }
            }
        });
        mWriter.endObject();

        mWriter.endObject();

        return this;
    }
}
