// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

import android.os.Debug.MemoryInfo;

import java.io.File;
import java.util.List;

/**
 * Wrapper for org.chromium.base.PerfTraceEvent to make landing cross-repository change easier. The
 * real PerfTraceEvent class used to be here TODO(aberent) - remove this.
 */

public class PerfTraceEvent {
    /**
     * Specifies what event names will be tracked.
     *
     * @param strings Event names we will record.
     */
    public static void setFilter(List<String> strings) {
        org.chromium.base.PerfTraceEvent.setFilter(strings);
    }

    /**
     * Enable or disable perf tracing. Disabling of perf tracing will dump trace data to the system
     * log.
     */
    public static void setEnabled(boolean enabled) {
        org.chromium.base.PerfTraceEvent.setEnabled(enabled);
    }

    /**
     * Enables memory tracking for all timing perf events tracked.
     *
     * <p>
     * Only works when called in combination with {@link #setEnabled(boolean)}.
     *
     * <p>
     * By enabling this feature, an additional perf event containing the memory usage will be logged
     * whenever {@link #instant(String)}, {@link #begin(String)}, or {@link #end(String)} is called.
     *
     * @param enabled Whether to enable memory tracking for all perf events.
     */
    public static void setMemoryTrackingEnabled(boolean enabled) {
        org.chromium.base.PerfTraceEvent.setMemoryTrackingEnabled(enabled);
    }

    /**
     * Enables timing tracking for all perf events tracked.
     *
     * <p>
     * Only works when called in combination with {@link #setEnabled(boolean)}.
     *
     * <p>
     * If this feature is enabled, whenever {@link #instant(String)}, {@link #begin(String)}, or
     * {@link #end(String)} is called the time since start of tracking will be logged.
     *
     * @param enabled Whether to enable timing tracking for all perf events.
     */
    public static synchronized void setTimingTrackingEnabled(boolean enabled) {
        org.chromium.base.PerfTraceEvent.setTimingTrackingEnabled(enabled);
    }

    /**
     * @return True if tracing is enabled, false otherwise. It is safe to call trace methods without
     *         checking if PerfTraceEvent is enabled.
     */
    public static boolean enabled() {
        return org.chromium.base.PerfTraceEvent.enabled();
    }

    /**
     * Record an "instant" perf trace event. E.g. "screen update happened".
     */
    public static synchronized void instant(String name) {
        org.chromium.base.PerfTraceEvent.instant(name);
    }


    /**
     * Record an "begin" perf trace event. Begin trace events should have a matching end event.
     */
    public static synchronized void begin(String name) {
        org.chromium.base.PerfTraceEvent.begin(name);
    }

    /**
     * Record an "end" perf trace event, to match a begin event. The time delta between begin and
     * end is usually interesting to graph code.
     */
    public static synchronized void end(String name) {
        org.chromium.base.PerfTraceEvent.end(name);
    }

    /**
     * Record an "begin" memory trace event. Begin trace events should have a matching end event.
     */
    public static void begin(String name, MemoryInfo memoryInfo) {
        org.chromium.base.PerfTraceEvent.begin(name, memoryInfo);
    }

    /**
     * Record an "end" memory trace event, to match a begin event. The memory usage delta between
     * begin and end is usually interesting to graph code.
     */
    public static synchronized void end(String name, MemoryInfo memoryInfo) {
        org.chromium.base.PerfTraceEvent.end(name, memoryInfo);
    }

    /**
     * Generating a trace name for tracking memory based on the timing name passed in.
     *
     * @param name The timing name to use as a base for the memory perf name.
     * @return The memory perf name to use.
     */
    public static String makeMemoryTraceNameFromTimingName(String name) {
        return org.chromium.base.PerfTraceEvent.makeMemoryTraceNameFromTimingName(name);
    }

    /**
     * Builds a name to be used in the perf trace framework. The framework has length requirements
     * for names, so this ensures the generated name does not exceed the maximum (trimming the base
     * name if necessary).
     *
     * @param baseName The base name to use when generating the name.
     * @param suffix The required suffix to be appended to the name.
     * @return A name that is safe for the perf trace framework.
     */
    public static String makeSafeTraceName(String baseName, String suffix) {
        return org.chromium.base.PerfTraceEvent.makeSafeTraceName(baseName, suffix);
    }

    /**
     * Sets a file to dump the results to. If {@code file} is {@code null}, it will be dumped to
     * STDOUT, otherwise the JSON performance data will be appended to {@code file}. This should be
     * called before the performance run starts. When {@link #setEnabled(boolean)} is called with
     * {@code false}, the perf data will be dumped.
     *
     * @param file Which file to append the performance data to. If {@code null}, the performance
     *        data will be sent to STDOUT.
     */
    public static synchronized void setOutputFile(File file) {
        org.chromium.base.PerfTraceEvent.setOutputFile(file);
    }
}
