// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.PrintWriter;
import java.util.HashMap;
import java.util.Map;

/**
 * Helper class for storing values to be written when a bugreport is taken.
 */
@JNINamespace("chromecast")
public final class DumpstateWriter {
    private Map<String, String> mDumpValues;

    private static DumpstateWriter sDumpstateWriter;

    public DumpstateWriter() {
        sDumpstateWriter = this;
        mDumpValues = new HashMap<>();
    }

    public void writeDumpValues(PrintWriter writer) {
        for (Map.Entry<String, String> entry : mDumpValues.entrySet()) {
            writer.println(entry.getKey() + ": " + entry.getValue());
        }
    }

    @CalledByNative
    private static void addDumpValue(String name, String value) {
        if (sDumpstateWriter == null) {
            throw new IllegalStateException(
                    "DumpstateWriter must be created before adding values.");
        }
        sDumpstateWriter.mDumpValues.put(name, value);
    }
}
