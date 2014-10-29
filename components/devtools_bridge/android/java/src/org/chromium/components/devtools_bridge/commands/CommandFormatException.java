// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.commands;

/**
 * This exception throws then a Command object cannot be reconstructed from serialized state.
 */
public class CommandFormatException extends Exception {
    public CommandFormatException(String message) {
        super(message);
    }

    public CommandFormatException(String message, Exception cause) {
        super(message, cause);
    }
}
