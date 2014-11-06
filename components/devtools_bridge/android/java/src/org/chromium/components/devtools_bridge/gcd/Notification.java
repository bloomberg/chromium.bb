// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.gcd;

import android.util.JsonReader;

import org.chromium.components.devtools_bridge.commands.Command;
import org.chromium.components.devtools_bridge.commands.CommandFormatException;

import java.io.IOException;
import java.io.StringReader;

/**
 * Notification that GCD sends to an instance.
 */
public final class Notification {
    public final String instanceId;
    public final Type type;
    public final Command command;

    public enum Type {
        COMMAND_CREATED, // Command created and needs to be executed.
        INSTANCE_UNREGISTERED // Instance unregistered (possibly through external UI).
    }

    Notification(String instanceId, Type type, Command command) {
        this.instanceId = instanceId;
        this.type = type;
        this.command = command;
    }

    public static Notification read(String source) throws FormatException {
        JsonReader reader = new JsonReader(new StringReader(source));
        try {
            Notification result = new MessageReader(reader).readNotification();
            reader.close();
            return result;
        } catch (CommandFormatException e) {
            throw new FormatException(e);
        } catch (IllegalStateException e) {
            throw new FormatException(e);
        } catch (IllegalArgumentException e) {
            throw new FormatException(e);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Exception when parsing notification.
     */
    public static class FormatException extends Exception {
        public FormatException(RuntimeException cause) {
            super(cause);
        }

        public FormatException(CommandFormatException cause) {
            super(cause);
        }
    }
}
