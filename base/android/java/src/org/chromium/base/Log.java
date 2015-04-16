// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.text.TextUtils;

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
 * remain and be executed. If they can't be avoided, try to generate the logs in a method annotated
 * with {@link NoSideEffects}. Another possibility is to use
 * {@link android.util.Log#isLoggable(String, int)}, with {@link Log#makeTag(String)} to get the
 * correct tag.
 * </p>
 *
 * Usage:
 * <pre>
 * private void myMethod(String awesome) {
 *   Log.i("Group", "My %s message.", awesome);
 *   Log.d("Group", "My debug message");
 * }
 * </pre>
 *
 * Logcat output:
 * <pre>
 * I/chromium.Group (999): My awesome message
 * D/chromium.Group (999): [MyClass.java:42] My debug message
 * </pre>
 *
 * Set the log level for a given group:
 * <pre>
 * $ adb shell setprop log.tag.chromium.Group VERBOSE
 * </pre>
 */
public class Log {
    private static final String BASE_TAG = "chromium";

    private Log() {
        // Static only access
    }

    /** Returns a formatted log message, using the supplied format and arguments.*/
    private static String formatLog(String messageTemplate, Object... params) {
        if (params != null && params.length != 0) {
            messageTemplate = String.format(Locale.US, messageTemplate, params);
        }

        return messageTemplate;
    }

    /**
     * Returns a formatted log message, using the supplied format and arguments.
     * The message will be prepended with the filename and line number of the call.
     */
    private static String formatLogWithStack(String messageTemplate, Object... params) {
        return "[" + getCallOrigin() + "] " + formatLog(messageTemplate, params);
    }

    /**
     * Returns a full tag for the provided group tag. Full tags longer than 23 characters
     * will cause a runtime exception later. (see {@link android.util.Log#isLoggable(String, int)})
     *
     * @param groupTag {@code null} and empty string are allowed.
     */
    public static String makeTag(String groupTag) {
        if (TextUtils.isEmpty(groupTag)) return BASE_TAG;
        return BASE_TAG + "." + groupTag;
    }

    /**
     * Sends a {@link android.util.Log#VERBOSE} log message.
     *
     * For optimization purposes, only the fixed parameters versions are visible. If you need more
     * than 7 parameters, consider building your log message using a function annotated with
     * {@link NoSideEffects}.
     *
     * @param groupTag Used to identify the source of a log message. It usually refers to the
     *                 package or feature, to logically group related logs.
     *
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string. If the last
     *             one is a {@link Throwable}, its trace will be printed.
     */
    private static void verbose(String groupTag, String messageTemplate, Object... args) {
        String tag = makeTag(groupTag);
        if (android.util.Log.isLoggable(tag, android.util.Log.VERBOSE)) {
            String message = formatLogWithStack(messageTemplate, args);
            Throwable tr = getThrowableToLog(args);
            if (tr != null) {
                android.util.Log.v(tag, message, tr);
            } else {
                android.util.Log.v(tag, message);
            }
        }
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 0 arg version. */
    public static void v(String groupTag, String message) {
        verbose(groupTag, message);
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 1 arg version. */
    public static void v(String groupTag, String messageTemplate, Object arg1) {
        verbose(groupTag, messageTemplate, arg1);
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 2 args version */
    public static void v(String groupTag, String messageTemplate, Object arg1, Object arg2) {
        verbose(groupTag, messageTemplate, arg1, arg2);
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 3 args version */
    public static void v(
            String groupTag, String messageTemplate, Object arg1, Object arg2, Object arg3) {
        verbose(groupTag, messageTemplate, arg1, arg2, arg3);
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 4 args version */
    public static void v(String groupTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4) {
        verbose(groupTag, messageTemplate, arg1, arg2, arg3, arg4);
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 5 args version */
    public static void v(String groupTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4, Object arg5) {
        verbose(groupTag, messageTemplate, arg1, arg2, arg3, arg4, arg5);
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 6 args version */
    public static void v(String groupTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4, Object arg5, Object arg6) {
        verbose(groupTag, messageTemplate, arg1, arg2, arg3, arg4, arg5, arg6);
    }

    /** Sends a {@link android.util.Log#VERBOSE} log message. 7 args version */
    public static void v(String groupTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4, Object arg5, Object arg6, Object arg7) {
        verbose(groupTag, messageTemplate, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    }

    /**
     * Sends a {@link android.util.Log#DEBUG} log message.
     *
     * For optimization purposes, only the fixed parameters versions are visible. If you need more
     * than 7 parameters, consider building your log message using a function annotated with
     * {@link NoSideEffects}.
     *
     * @param groupTag Used to identify the source of a log message. It usually refers to the
     *                 package or feature, to logically group related logs.
     *
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string. If the last
     *             one is a {@link Throwable}, its trace will be printed.
     */
    private static void debug(String groupTag, String messageTemplate, Object... args) {
        String tag = makeTag(groupTag);
        if (android.util.Log.isLoggable(tag, android.util.Log.VERBOSE)) {
            String message = formatLogWithStack(messageTemplate, args);
            Throwable tr = getThrowableToLog(args);
            if (tr != null) {
                android.util.Log.d(tag, message, tr);
            } else {
                android.util.Log.d(tag, message);
            }
        }
    }

    /** Sends a {@link android.util.Log#DEBUG} log message. 0 arg version. */
    public static void d(String groupTag, String message) {
        debug(groupTag, message);
    }

    /** Sends a {@link android.util.Log#DEBUG} log message. 1 arg version. */
    public static void d(String groupTag, String messageTemplate, Object arg1) {
        debug(groupTag, messageTemplate, arg1);
    }
    /** Sends a {@link android.util.Log#DEBUG} log message. 2 args version */
    public static void d(String groupTag, String messageTemplate, Object arg1, Object arg2) {
        debug(groupTag, messageTemplate, arg1, arg2);
    }
    /** Sends a {@link android.util.Log#DEBUG} log message. 3 args version */
    public static void d(
            String groupTag, String messageTemplate, Object arg1, Object arg2, Object arg3) {
        debug(groupTag, messageTemplate, arg1, arg2, arg3);
    }

    /** Sends a {@link android.util.Log#DEBUG} log message. 4 args version */
    public static void d(String groupTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4) {
        debug(groupTag, messageTemplate, arg1, arg2, arg3, arg4);
    }

    /** Sends a {@link android.util.Log#DEBUG} log message. 5 args version */
    public static void d(String groupTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4, Object arg5) {
        debug(groupTag, messageTemplate, arg1, arg2, arg3, arg4, arg5);
    }

    /** Sends a {@link android.util.Log#DEBUG} log message. 6 args version */
    public static void d(String groupTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4, Object arg5, Object arg6) {
        debug(groupTag, messageTemplate, arg1, arg2, arg3, arg4, arg5, arg6);
    }

    /** Sends a {@link android.util.Log#DEBUG} log message. 7 args version */
    public static void d(String groupTag, String messageTemplate, Object arg1, Object arg2,
            Object arg3, Object arg4, Object arg5, Object arg6, Object arg7) {
        debug(groupTag, messageTemplate, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    }

    /**
     * Sends an {@link android.util.Log#INFO} log message.
     *
     * @param groupTag Used to identify the source of a log message. It usually refers to the
     *                 package or feature, to logically group related logs.
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string. If the last
     *             one is a {@link Throwable}, its trace will be printed.
     */
    public static void i(String groupTag, String messageTemplate, Object... args) {
        String tag = makeTag(groupTag);
        if (android.util.Log.isLoggable(tag, android.util.Log.INFO)) {
            String message = formatLog(messageTemplate, args);
            Throwable tr = getThrowableToLog(args);
            if (tr != null) {
                android.util.Log.i(tag, message, tr);
            } else {
                android.util.Log.i(tag, message);
            }
        }
    }

    /**
     * Sends a {@link android.util.Log#WARN} log message.
     *
     * @param groupTag Used to identify the source of a log message. It usually refers to the
     *                 package or feature, to logically group related logs.
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string. If the last
     *             one is a {@link Throwable}, its trace will be printed.
     */
    public static void w(String groupTag, String messageTemplate, Object... args) {
        String tag = makeTag(groupTag);
        if (android.util.Log.isLoggable(tag, android.util.Log.WARN)) {
            String message = formatLog(messageTemplate, args);
            Throwable tr = getThrowableToLog(args);
            if (tr != null) {
                android.util.Log.w(tag, message, tr);
            } else {
                android.util.Log.w(tag, message);
            }
        }
    }

    /**
     * Sends an {@link android.util.Log#ERROR} log message.
     *
     * @param groupTag Used to identify the source of a log message. It usually refers to the
     *                 package or feature, to logically group related logs.
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string. If the last
     *             one is a {@link Throwable}, its trace will be printed.
     */
    public static void e(String groupTag, String messageTemplate, Object... args) {
        String tag = makeTag(groupTag);
        if (android.util.Log.isLoggable(tag, android.util.Log.ERROR)) {
            String message = formatLog(messageTemplate, args);
            Throwable tr = getThrowableToLog(args);
            if (tr != null) {
                android.util.Log.e(tag, message, tr);
            } else {
                android.util.Log.e(tag, message);
            }
        }
    }

    /**
     * What a Terrible Failure: Used for conditions that should never happen, and logged at
     * the {@link android.util.Log#ASSERT} level. Depending on the configuration, it might
     * terminate the process.
     *
     * @see android.util.Log#wtf(String, String, Throwable)
     *
     * @param groupTag Used to identify the source of a log message. It usually refers to the
     *                 package or feature, to logically group related logs.
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string. If the last
     *             one is a {@link Throwable}, its trace will be printed.
     */
    public static void wtf(String groupTag, String messageTemplate, Object... args) {
        String tag = makeTag(groupTag);
        if (android.util.Log.isLoggable(tag, android.util.Log.ERROR)) {
            String message = formatLog(messageTemplate, args);
            Throwable tr = getThrowableToLog(args);
            if (tr != null) {
                android.util.Log.wtf(tag, message, tr);
            } else {
                android.util.Log.wtf(tag, message);
            }
        }
    }

    private static Throwable getThrowableToLog(Object[] args) {
        if (args == null || args.length == 0) return null;

        Object lastArg = args[args.length - 1];

        if (!(lastArg instanceof Throwable)) return null;
        return (Throwable) lastArg;
    }

    /** Returns a string form of the origin of the log call, to be used as secondary tag.*/
    private static String getCallOrigin() {
        StackTraceElement[] st = Thread.currentThread().getStackTrace();

        // The call stack should look like:
        //   n [a variable number of calls depending on the vm used]
        //  +0 getCallOrigin()
        //  +1 privateLogFunction: verbose or debug
        //  +2 formatLogWithStack()
        //  +3 logFunction: v or d
        //  +4 caller

        int callerStackIndex;
        String logClassName = Log.class.getName();
        for (callerStackIndex = 0; callerStackIndex < st.length; callerStackIndex++) {
            if (st[callerStackIndex].getClassName().equals(logClassName)) {
                callerStackIndex += 4;
                break;
            }
        }

        return st[callerStackIndex].getFileName() + ":" + st[callerStackIndex].getLineNumber();
    }
}
