// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.commands;

import java.util.List;
import java.util.Map;

/**
 * Definition for a Command and a command factory. Definition needed when
 * registering in GCD. GCD only interested in in-parameters so to CommandDefinition.
 */
public abstract class CommandDefinition {
    private final String mName;
    private final List<ParamDefinition<?>> mInParams;

    public CommandDefinition(String name, List<ParamDefinition<?>> inParams) {
        mName = name;
        mInParams = inParams;
    }

    public Iterable<ParamDefinition<?>> inParams() {
        return mInParams;
    }

    public String shortName() {
        return "_" + mName;
    }

    public String fullName() {
        return "base._" + mName;
    }

    /**
     * Factory method for creaiting a command from serialized state.
     */
    public abstract Command newCommand(String id, Map<String, String> actualParameters)
            throws CommandFormatException;
}
