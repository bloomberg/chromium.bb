// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

/**
 * Wrapper for org.chromium.base.TraceEvent to make landing cross-repository
 * change easier. The real TraceEvent class used to be here
 * TODO(aberent) - remove this.
 */
public class TraceEvent {

    /**
     * Calling this will cause enabled() to be updated to match that set on the native side.
     * The native library must be loaded before calling this method.
     */
    public static void setEnabledToMatchNative() {
        org.chromium.base.TraceEvent.setEnabledToMatchNative();
    }

    /**
     * Enables or disables tracing.
     * The native library must be loaded before the first call with enabled == true.
     */
    public static void setEnabled(boolean enabled) {
        org.chromium.base.TraceEvent.setEnabled(enabled);
   }

    /**
     * @return True if tracing is enabled, false otherwise.
     * It is safe to call trace methods without checking if TraceEvent
     * is enabled.
     */
    public static boolean enabled() {
        return org.chromium.base.TraceEvent.enabled();
    }

    /**
     * Triggers the 'instant' native trace event with no arguments.
     * @param name The name of the event.
     */
    public static void instant(String name) {
        org.chromium.base.TraceEvent.instant(name);
    }

    /**
     * Triggers the 'instant' native trace event.
     * @param name The name of the event.
     * @param arg  The arguments of the event.
     */
    public static void instant(String name, String arg) {
        org.chromium.base.TraceEvent.instant(name, arg);
    }

    /**
     * Convenience wrapper around the versions of startAsync() that take string parameters.
     * @param id The id of the asynchronous event.  Will automatically figure out the name from
     *           calling {@link #getCallerName()}.
     * @see #begin()
     */
    public static void startAsync(long id) {
        org.chromium.base.TraceEvent.startAsync(id);
    }

    /**
     * Triggers the 'start' native trace event with no arguments.
     * @param name The name of the event.
     * @param id   The id of the asynchronous event.
     * @see #begin()
     */
    public static void startAsync(String name, long id) {
        org.chromium.base.TraceEvent.startAsync(name, id);
    }

    /**
     * Triggers the 'start' native trace event.
     * @param name The name of the event.
     * @param id   The id of the asynchronous event.
     * @param arg  The arguments of the event.
     * @see #begin()
     */
    public static void startAsync(String name, long id, String arg) {
        org.chromium.base.TraceEvent.startAsync(name, id, arg);
    }

    /**
     * Convenience wrapper around the versions of finishAsync() that take string parameters.
     * @param id The id of the asynchronous event.  Will automatically figure out the name from
     *           calling {@link #getCallerName()}.
     * @see #finish()
     */
    public static void finishAsync(long id) {
        org.chromium.base.TraceEvent.finishAsync(id);
    }

    /**
     * Triggers the 'finish' native trace event with no arguments.
     * @param name The name of the event.
     * @param id   The id of the asynchronous event.
     * @see #begin()
     */
    public static void finishAsync(String name, long id) {
        org.chromium.base.TraceEvent.finishAsync(name,id);
    }

    /**
     * Triggers the 'finish' native trace event.
     * @param name The name of the event.
     * @param id   The id of the asynchronous event.
     * @param arg  The arguments of the event.
     * @see #begin()
     */
    public static void finishAsync(String name, long id, String arg) {
        org.chromium.base.TraceEvent.finishAsync(name, id, arg);
    }

    /**
     * Convenience wrapper around the versions of begin() that take string parameters.
     * The name of the event will be derived from the class and function name that call this.
     * IMPORTANT: if using this version, ensure end() (no parameters) is always called from the
     * same calling context.
     */
    public static void begin() {
        org.chromium.base.TraceEvent.begin();
    }

    /**
     * Triggers the 'begin' native trace event with no arguments.
     * @param name The name of the event.
     */
    public static void begin(String name) {
        org.chromium.base.TraceEvent.begin(name);
    }

    /**
     * Triggers the 'begin' native trace event.
     * @param name The name of the event.
     * @param arg  The arguments of the event.
     */
    public static void begin(String name, String arg) {
        org.chromium.base.TraceEvent.begin(name, arg);
    }

    /**
     * Convenience wrapper around the versions of end() that take string parameters. See begin()
     * for more information.
     */
    public static void end() {
        org.chromium.base.TraceEvent.end();
    }

    /**
     * Triggers the 'end' native trace event with no arguments.
     * @param name The name of the event.
     */
    public static void end(String name) {
        org.chromium.base.TraceEvent.end(name);
    }

    /**
     * Triggers the 'end' native trace event.
     * @param name The name of the event.
     * @param arg  The arguments of the event.
     */
    public static void end(String name, String arg) {
        org.chromium.base.TraceEvent.end(name, arg);
    }
}
