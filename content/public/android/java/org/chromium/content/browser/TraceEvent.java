// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Looper;
import android.util.Printer;

// Java mirror of Chrome trace event API.  See
// base/debug/trace_event.h.  Unlike the native version, Java does not
// have stack objects, so a TRACE_EVENT() which does both
// TRACE_EVENT_BEGIN() and TRACE_EVENT_END() in ctor/dtor is not
// possible.
// It is OK to use tracing before the native library has loaded, but such traces will
// be ignored. (Perhaps we could devise to buffer them up in future?).
public class TraceEvent {

    private static boolean sEnabled = false;

    private static class LooperTracePrinter implements Printer {
        private static final String NAME = "Looper.dispatchMessage";
        @Override
        public void println(String line) {
            if (line.startsWith(">>>>>")) {
                TraceEvent.begin(NAME, line);
            } else {
                assert line.startsWith("<<<<<");
                TraceEvent.end(NAME);
            }
        }
    }

    /**
     * Calling this will cause enabled() to be updated to match that set on the native side.
     * The native library must be loaded before calling this method.
     */
    public static void setEnabledToMatchNative() {
        setEnabled(nativeTraceEnabled());
    }

    /**
     * Enables or disables tracing.
     * The native library must be loaded before the first call with enabled == true.
     */
    public static synchronized void setEnabled(boolean enabled) {
        if (enabled) {
            LibraryLoader.checkIsReady();
        }
        if (sEnabled == enabled) {
            return;
        }
        sEnabled = enabled;
        Looper.getMainLooper().setMessageLogging(enabled ? new LooperTracePrinter() : null);
    }

    /**
     * @return True if tracing is enabled, false otherwise.
     * It is safe to call trace methods without checking if TraceEvent
     * is enabled.
     */
    public static boolean enabled() {
        return sEnabled;
    }

    public static void instant(String name) {
        if (sEnabled) {
            nativeInstant(name, null);
        }
    }

    public static void instant(String name, String arg) {
        if (sEnabled) {
            nativeInstant(name, arg);
        }
    }

    /**
     * Convenience wrapper around the versions of begin() that take string parameters.
     * The name of the event will be derived from the class and function name that call this.
     * IMPORTANT: if using this version, ensure end() (no parameters) is always called from the
     * same calling context.
     */
    public static void begin() {
        if (sEnabled) {
            nativeBegin(getCallerName(), null);
        }
    }

    public static void begin(String name) {
        if (sEnabled) {
            nativeBegin(name, null);
        }
    }

    public static void begin(String name, String arg) {
        if (sEnabled) {
            nativeBegin(name, arg);
        }
    }

    /**
     * Convenience wrapper around the versions of end() that take string parameters. See begin()
     * for more information.
     */
    public static void end() {
        if (sEnabled) {
            nativeEnd(getCallerName(), null);
        }
    }

    public static void end(String name) {
        if (sEnabled) {
            nativeEnd(name, null);
        }
    }

    public static void end(String name, String arg) {
        if (sEnabled) {
            nativeEnd(name, arg);
        }
    }

    private static String getCallerName() {
        // This was measured to take about 1ms on Trygon device.
        StackTraceElement[] stack = java.lang.Thread.currentThread().getStackTrace();

        // Commented out to avoid excess call overhead, but these lines can be useful to debug
        // exactly where the TraceEvent's client is on the callstack.
        //  int index = 0;
        //  while (!stack[index].getClassName().equals(TraceEvent.class.getName())) ++index;
        //  while (stack[index].getClassName().equals(TraceEvent.class.getName())) ++index;
        //  System.logW("TraceEvent caller is at stack index " + index);

        // '4' Was derived using the above commented out code snippet.
        return stack[4].getClassName() + "." + stack[4].getMethodName();
    }

    private static native boolean nativeTraceEnabled();
    private static native void nativeInstant(String name, String arg);
    private static native void nativeBegin(String name, String arg);
    private static native void nativeEnd(String name, String arg);
}
