// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge.util;

import android.util.JsonReader;
import android.util.JsonWriter;

import java.io.StringReader;
import java.io.StringWriter;

/**
 * Helper class for testing JSON-based readers.
 */
public class TestSource {
    private final StringWriter mSource;
    private final JsonWriter mSourceWriter;

    public TestSource() {
        mSource = new StringWriter();
        mSourceWriter = new JsonWriter(mSource);
    }

    public JsonReader read() {
        return new JsonReader(new StringReader(mSource.toString()));
    }

    public JsonWriter write() {
        return mSourceWriter;
    }
}
