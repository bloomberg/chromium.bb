// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
* JNI call glue for AutofillExternalDelagate C++ and Java objects.
*/
@JNINamespace("autofill")
public class AutofillLogger {
    /**
     * An entry to be sent to Logger.
     */
    public static class LogEntry {
        private final String mAutofilledValue;
        private final String mProfileFullName;

        private LogEntry(String autofilledValue, String profileFullName) {
            mAutofilledValue = autofilledValue;
            mProfileFullName = profileFullName;
        }

        public String getAutofilledValue() {
            return mAutofilledValue;
        }

        public String getProfileFullName() {
            return mProfileFullName;
        }
    }

    /**
     * A logger interface. Uses LogItem instead of individual fields to allow
     * changing the items that are logged without breaking the embedder.
     */
    public interface Logger {
        public void didFillField(LogEntry logItem);
    }

    private static Logger sLogger = null;

    public static void setLogger(Logger logger) {
        sLogger = logger;
    }

    @CalledByNative
    private static void didFillField(String autofilledValue, String profileFullName) {
        if (sLogger == null) return;
        sLogger.didFillField(new LogEntry(autofilledValue, profileFullName));
    }
}
