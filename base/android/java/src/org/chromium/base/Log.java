// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.base.annotations.NoSideEffects;

import java.util.Locale;

/**
 * Utility class for Logging.
 *
 * <p>
 * Defines logging access points for each feature. They format and forward the logs to
 * {@link android.util.Log}, allowing to standardize the output, to make it easy to identify
 * the origin of logs, and enable or disable logging in different parts of the code.
 * </p>
 * <p>
 * Please make use of the formatting capability of the logging methods rather than doing
 * concatenations in the calling code. In the release builds of Chrome, debug and verbose log
 * calls will be stripped out of the binary. Concatenations and method calls however will still
 * remain and be executed. If they can't be avoided, use {@link Log#isEnabled(int)} to guard
 * such calls. Another possibility is to annotate methods to be called with {@link NoSideEffects}.
 * </p>
 *
 * Usage:
 * <pre>
 * Log.FEATURE.d("MyTag", "My %s message", awesome);
 * </pre>
 *
 * Logcat output:
 * <pre>
 * D/chromium.Feature (999): [MyTag] My awesome message
 * </pre>
 *
 * Set the log level for a given feature:
 * <pre>
 * adb shell setprop log.tag.chromium:Feature VERBOSE
 * </pre>
 *
 *<p>
 *<b>Notes:</b>
 * <ul>
 * <li>For loggers configured to log the origin of debug calls (see {@link #Log(String, boolean)},
 * the tag provided for debug and verbose calls will be ignored and replaced in the log with
 * the file name and the line number.</li>
 * <li>New features or features not having a dedicated logger: please make a new one rather
 * than using {@link #ROOT}.</li>
 * </ul>
 * </p>
 */
public class Log {
    private static final String BASE_TAG = "chromium";

    /**
     * Maximum length for the feature tag.
     *
     * A complete tag will look like <code>chromium:FooFeature</code>. Because of the 23 characters
     * limit on log tags, feature tags have to be restricted to fit.
     */
    private static final int MAX_FEATURE_TAG_LENGTH = 23 - 1 - BASE_TAG.length();

    /**
     * Logger for the "chromium" tag.
     * Note: Disabling logging for that one will not disable the others.
     */
    public static final Log ROOT = new Log(null, true);

    @VisibleForTesting
    final String mTag;
    private final boolean mDebugWithStack;

    /**
     * Creates a new logging access point for the given tag.
     * @param featureTag The complete log tag will be displayed as "chromium.featureTag".
     *                   If <code>null</code>, it will only be "chromium".
     * @param debugWithStack Whether to replace the secondary tag name with the file name and line
     *                       number of the origin of the call for debug and verbose logs.
     * @throws IllegalArgumentException If <code>featureTag</code> is too long. The complete
     *                                  tag has to fit within 23 characters.
     */
    protected Log(String featureTag, boolean debugWithStack) {
        mDebugWithStack = debugWithStack;
        if (featureTag == null) {
            mTag = BASE_TAG;
            return;
        } else if (featureTag.length() > MAX_FEATURE_TAG_LENGTH) {
            throw new IllegalArgumentException(
                    "The feature tag can be at most " + MAX_FEATURE_TAG_LENGTH + " characters.");
        } else {
            mTag = BASE_TAG + "." + featureTag;
        }
    }

    /** Returns whether this logger is currently allowed to send logs.*/
    public boolean isEnabled(int level) {
        return android.util.Log.isLoggable(mTag, level);
    }

    /** Returns a formatted log message, using the supplied format and arguments.*/
    @VisibleForTesting
    protected String formatLog(String secondaryTag, String messageTemplate, Object... params) {
        if (params != null && params.length != 0) {
            messageTemplate = String.format(Locale.US, messageTemplate, params);
        }

        return "[" + secondaryTag + "] " + messageTemplate;
    }

    /**
     * Sends a {@link android.util.Log#VERBOSE} log message.
     *
     * For optimization purposes, only the fixed parameters versions are visible. If you need more
     * than 7 parameters, consider building your log message using a function annotated with
     * {@link NoSideEffects}.
     *
     * @param secondaryTag Used to identify the source of a log message. It usually identifies the
     *                     class where the log call occurs. If the logger is configured to log the
     *                     call's origin (see {@link #Log(String, boolean)}, this parameter is
     *                     unused and will be replaced in the log message with the file name and
     *                     the line number.
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string.
     */
    private void verbose(String secondaryTag, String messageTemplate, Object... args) {
        if (isEnabled(android.util.Log.VERBOSE)) {
            if (mDebugWithStack) secondaryTag = getCallOrigin();
            android.util.Log.v(mTag, formatLog(secondaryTag, messageTemplate, args));
        }
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 0 arg version. */
    public void v(String secondaryTag, String message) {
        verbose(secondaryTag, message);
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 1 arg version. */
    public void v(String secondaryTag, String messageTemplate, Object arg1) {
        verbose(secondaryTag, messageTemplate, arg1);
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 2 args version */
    public void v(String secondaryTag, String messageTemplate, Object arg1, Object arg2) {
        verbose(secondaryTag, messageTemplate, arg1, arg2);
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 3 args version */
    public void v(
            String secondaryTag, String messageTemplate, Object arg1, Object arg2, Object arg3) {
        verbose(secondaryTag, messageTemplate, arg1, arg2, arg3);
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 4 args version */
    public void v(String secondaryTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4) {
        verbose(secondaryTag, messageTemplate, arg1, arg2, arg3, arg4);
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 5 args version */
    public void v(String secondaryTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4, Object arg5) {
        verbose(secondaryTag, messageTemplate, arg1, arg2, arg3, arg4, arg5);
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 6 args version */
    public void v(String secondaryTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4, Object arg5, Object arg6) {
        verbose(secondaryTag, messageTemplate, arg1, arg2, arg3, arg4, arg5, arg6);
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 7 args version */
    public void v(String secondaryTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4, Object arg5, Object arg6, Object arg7) {
        verbose(secondaryTag, messageTemplate, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    }

    /**
     * Sends a {@link android.util.Log#DEBUG} log message.
     *
     * For optimization purposes, only the fixed parameters versions are visible. If you need more
     * than 7 parameters, consider building your log message using a function annotated with
     * {@link NoSideEffects}.
     *
     * @param secondaryTag Used to identify the source of a log message. It usually identifies the
     *                     class where the log call occurs. If the logger is configured to log the
     *                     call's origin (see {@link #Log(String, boolean)}, this parameter is
     *                     unused and will be replaced in the log message with the file name and
     *                     the line number.
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string.
     */
    private void debug(String secondaryTag, String messageTemplate, Object... args) {
        if (isEnabled(android.util.Log.DEBUG)) {
            if (mDebugWithStack) secondaryTag = getCallOrigin();
            android.util.Log.d(mTag, formatLog(secondaryTag, messageTemplate, args));
        }
    }

    /** Sends a {@link android.util.Log#DEBUG} log message. 0 arg version. */
    public void d(String secondaryTag, String message) {
        debug(secondaryTag, message);
    }

    /** Sends a {@link android.util.Log#DEBUG} log message. 1 arg version. */
    public void d(String secondaryTag, String messageTemplate, Object arg1) {
        debug(secondaryTag, messageTemplate, arg1);
    }
    /** Sends a {@link android.util.Log#DEBUG} log message. 2 args version */
    public void d(String secondaryTag, String messageTemplate, Object arg1, Object arg2) {
        debug(secondaryTag, messageTemplate, arg1, arg2);
    }
    /** Sends a {@link android.util.Log#DEBUG} log message. 3 args version */
    public void d(
            String secondaryTag, String messageTemplate, Object arg1, Object arg2, Object arg3) {
        debug(secondaryTag, messageTemplate, arg1, arg2, arg3);
    }

    /** Sends a {@link android.util.Log#DEBUG} log message. 4 args version */
    public void d(String secondaryTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4) {
        debug(secondaryTag, messageTemplate, arg1, arg2, arg3, arg4);
    }

    /** Sends a {@link android.util.Log#DEBUG} log message. 5 args version */
    public void d(String secondaryTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4, Object arg5) {
        debug(secondaryTag, messageTemplate, arg1, arg2, arg3, arg4, arg5);
    }

    /** Sends a {@link android.util.Log#DEBUG} log message. 6 args version */
    public void d(String secondaryTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4, Object arg5, Object arg6) {
        debug(secondaryTag, messageTemplate, arg1, arg2, arg3, arg4, arg5, arg6);
    }

    /** Sends a {@link android.util.Log#DEBUG} log message. 7 args version */
    public void d(String secondaryTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4, Object arg5, Object arg6, Object arg7) {
        debug(secondaryTag, messageTemplate, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    }

    /**
     * Sends an {@link android.util.Log#INFO} log message.
     *
     * @param secondaryTag Used to identify the source of a log message. It usually identifies the
     *                     class where the log call occurs.
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string.
     */
    public void i(String secondaryTag, String messageTemplate, Object... args) {
        if (isEnabled(android.util.Log.INFO)) {
            android.util.Log.i(mTag, formatLog(secondaryTag, messageTemplate, args));
        }
    }

    /**
     * Sends a {@link android.util.Log#WARN} log message.
     *
     * @param secondaryTag Used to identify the source of a log message. It usually identifies the
     *                     class where the log call occurs.
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string.
     */
    public void w(String secondaryTag, String messageTemplate, Object... args) {
        if (isEnabled(android.util.Log.WARN)) {
            android.util.Log.w(mTag, formatLog(secondaryTag, messageTemplate, args));
        }
    }

    /**
     * Sends an {@link android.util.Log#ERROR} log message.
     *
     * @param secondaryTag Used to identify the source of a log message. It usually identifies the
     *                     class where the log call occurs.
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string.
     */
    public void e(String secondaryTag, String messageTemplate, Object... args) {
        if (isEnabled(android.util.Log.ERROR)) {
            android.util.Log.e(mTag, formatLog(secondaryTag, messageTemplate, args));
        }
    }

    /** Returns a string form of the origin of the log call, to be used as secondary tag.*/
    private String getCallOrigin() {
        StackTraceElement[] st = Thread.currentThread().getStackTrace();

        // The call stack should look like:
        //   n [a variable number of calls depending on the vm used]
        //  +0 getCallOrigin()
        //  +1 privateLogFunction: verbose or debug
        //  +2 logFunction: v or d
        //  +3 caller

        int callerStackIndex;
        String logClassName = Log.class.getName();
        for (callerStackIndex = 0; callerStackIndex < st.length; callerStackIndex++) {
            if (st[callerStackIndex].getClassName().equals(logClassName)) {
                callerStackIndex += 3;
                break;
            }
        }

        return st[callerStackIndex].getFileName() + ":" + st[callerStackIndex].getLineNumber();
    }
}
