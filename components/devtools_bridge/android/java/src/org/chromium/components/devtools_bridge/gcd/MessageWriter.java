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
public final class MessageWriter {
    private final StringWriter mStringWriter;
    private final JsonWriter mWriter;
    boolean mClosed = false;

    public MessageWriter() {
        mStringWriter = new StringWriter();
        mWriter = new JsonWriter(mStringWriter);
    }

    public MessageWriter close() throws IOException {
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

    /**
     * Writes body of registrationTicket PATCH request.
     */
    public MessageWriter writeTicketPatch(InstanceDescription description) throws IOException {
        mWriter.beginObject();
        mWriter.name("deviceDraft");
        writeDeviceDraft(description);
        mWriter.name("oauthClientId").value(description.oAuthClientId);
        mWriter.endObject();
        return this;
    }

    /**
     * Writes body of devices/<instanceId> PATCH request.
     */
    public MessageWriter writeDeviceGCMChannelPatch(String gcmChannelId) throws IOException {
        mWriter.beginObject();
        mWriter.name("channel");
        mWriter.beginObject();
        mWriter.name("gcmRegistrationId").value(gcmChannelId);
        mWriter.endObject();
        mWriter.endObject();
        return this;
    }

    private void writeDeviceDraft(InstanceDescription description) throws IOException {
        mWriter.beginObject();
        mWriter.name("deviceKind").value("vendor");
        mWriter.name("displayName").value(description.displayName);
        mWriter.name("systemName").value("Chrome DevTools Bridge");
        mWriter.name("channel");
        writeChannelDefinition(description);
        mWriter.name("commandDefs");
        writeCommandsDefinition();
        mWriter.endObject();
    }

    private void writeChannelDefinition(InstanceDescription description) throws IOException {
        mWriter.beginObject();
        mWriter.name("supportedType").value("gcm");
        mWriter.name("gcmRegistrationId").value(description.gcmChannelId);
        mWriter.endObject();
    }

    private void writeCommandsDefinition() throws IOException {
        mWriter.beginObject();
        mWriter.name("base");
        writeCommandsDefinitionBase();
        mWriter.endObject();
    }

    private void writeCommandsDefinitionBase() throws IOException {
        mWriter.beginObject();
        for (Command.Type type : Command.Type.values()) {
            mWriter.name(type.definition.shortName());
            beginParameters();
            for (ParamDefinition<?> param : type.definition.inParams()) {
                writeParameter(param.name(), param.type());
            }
            endParameters();
        }
        mWriter.endObject();
    }

    private void beginParameters() throws IOException {
        mWriter.beginObject();
        mWriter.name("parameters");
        mWriter.beginObject();
    }

    private void endParameters() throws IOException {
        mWriter.endObject();
        mWriter.endObject();
    }

    private void writeParameter(String name, String type) throws IOException {
        mWriter.name(name);
        mWriter.beginObject();
        mWriter.name("type").value(type);
        mWriter.endObject();
    }

    /**
     * Writes body of command PATCH request. Updates command status and out parametes
     * when the command has processed.
     */
    public MessageWriter writeCommandPatch(Command command) throws IOException {
        mWriter.beginObject();
        if (command.state() == Command.State.DONE) {
            mWriter.name("state").value("done");
            mWriter.name("results");
            mWriter.beginObject();
            command.visitOutParams(new ParamWriter());
            mWriter.endObject();
        } else if (command.state() == Command.State.ERROR) {
            mWriter.name("state").value("error");
            mWriter.name("error");
            mWriter.beginObject();
            mWriter.name("message").value(command.getErrorMessage());
            mWriter.endObject();
        }
        mWriter.endObject();
        return this;
    }

    private class ParamWriter implements Command.ParamVisitor {
        @Override
        public void visit(ParamDefinition<?> param, String value) {
            try {
                mWriter.name(param.name()).value(value);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
    }
}
