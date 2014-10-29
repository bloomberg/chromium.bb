// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.commands;

import java.util.Map;

/**
 * Desribes in- or out-parameter of a Command.
 * @param <T> Type of the parameter.
 */
public abstract class ParamDefinition<T> {
    public final String mName;

    protected ParamDefinition(String name) {
        mName = name;
    }

    public String name() {
        return "_" + mName;
    }

    public String type() {
        return "string";
    }

    public void checkPresents(Map<String, String> actualParameters) throws CommandFormatException {
        if (!actualParameters.containsKey(name())) {
            throw new CommandFormatException("Missing parameter " + mName);
        }
    }

    public T get(Map<String, String> actualParameters) throws CommandFormatException {
        return fromString(actualParameters.get(name()));
    }

    public T checkAndGet(Map<String, String> actualParameters) throws CommandFormatException {
        checkPresents(actualParameters);
        return get(actualParameters);
    }

    public final void pass(Command.ParamVisitor visitor, T value) {
        visitor.visit(this, toString(value));
    }

    protected abstract T fromString(String value) throws CommandFormatException;
    protected abstract String toString(T value);
}
