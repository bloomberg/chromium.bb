// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.concurrent.atomic.AtomicReference;

// Java mirror of Chrome command-line utilities (e.g. class CommandLine from base/command_line.h).
// Command line program adb_command_line can be used to set the Chrome command line:
// adb shell "echo chrome --my-param > /data/local/chrome-command-line"

// TODO(jrg), TODO(tedchoc): some of these command line options are
// specific to chrome, not content.  transition them to a different
// location.

public abstract class CommandLine {
    // Switches used from Java.  Please continue switch style used Chrome where options-have-hypens
    // and are_not_split_with_underscores.

    // Block onCreate() of Chrome until a Java debugger is attached.
    public static final String WAIT_FOR_JAVA_DEBUGGER = "wait-for-java-debugger";

    // Disable instant?
    public static final String DISABLE_INSTANT = "disable-instant";

    // Should we force-enable the "hardware acceleration" preference?
    public static final String HARDWARE_ACCELERATION = "hardware-acceleration";

    // Give an extra account name for the x-auto-login spinner, for testing.
    public static final String AUTO_LOGIN_EXTRA_ACCOUNT = "auto-login-extra-account";

    // Enable exact match x-auto-login?  Disabled for now (see
    // AccountManagerContainer.java).
    public static final String EXACT_MATCH_AUTO_LOGIN = "exact-match-auto-login";

    // Should we show the NTP cache menu items?
    public static final String CACHED_NTP_MENU = "cached-ntp-menu";

    // Should we use a preload web view container for the ntp?
    public static final String PRELOAD_WEBVIEW_CONTAINER = "preload-webview-container";

    // Should thumbnail bitmaps for the thumbnail cache be full size?
    public static final String FULL_SIZE_THUMBNAILS = "full-size-thumbnails";

    // Should thumbnail bitmaps for the thumbnail cache be a compromise size?
    public static final String COMPROMISE_SIZE_THUMBNAILS = "compromise-size-thumbnails";

    // Should thumbnail bitmaps for the thumbnail cache be small (1/4 size)?
    // Uses less memory and is faster but has a significant quality reduction.
    public static final String SMALL_SIZE_THUMBNAILS = "small-size-thumbnails";

    // Should thumbnail bitmaps be generated at 16-bit?  32-bit?
    public static final String SIXTEEN_BIT_THUMBNAILS = "sixteen-bit-thumbnails";
    public static final String THIRTYTWO_BIT_THUMBNAILS = "thirtytwo-bit-thumbnails";

    // How many thumbnails should we allow in the cache (per tab stack)?
    public static final String THUMBNAILS = "thumbnails";

    // How many "approximated" thumbnails should we allow in the cache
    // (per tab stack)?  These take very low memory but have poor quality.
    public static final String APPROXIMATION_THUMBNAILS = "approximation-thumbnails";

    // What scaling to use for the approximation?  Integer.
    public static final String APPROXIMATION_SCALE = "approximation-scale";

    // Tell Java to use the official command line, loaded from the
    // official-command-line.xml files.  WARNING this is not done
    // immediately on startup, so early running Java code will not see
    // these flags.
    public static final String ADD_OFFICIAL_COMMAND_LINE = "add-official-command-line";

    // If specified, enables notification center verbose logging.
    public static final String NOTIFICATION_CENTER_LOGGING = "notification-center-logging";

    // Enables test intent handling.
    public static final String ENABLE_TEST_INTENTS = "enable-test-intents";

    // Override the system fling friction (0.015f). This can make it easier to see scrolling bugs.
    public static final String FLING_FRICTION = "fling-friction";

    // Enables StrictMode violation detection. By default this logs violations to logcat.
    public static final String STRICT_MODE = "strict-mode";

    // Enable the First Run Experience
    // TODO(dtrainor): This should be removed eventually, as it is only used for development and
    // debugging.
    public static final String ENABLE_FIRST_RUN_EXPERIENCE = "enable-fre";

    // Disable the First Run Experience
    // TODO(dtrainor): This should be removed eventually, as it is only used for development and
    // debugging.
    public static final String DISABLE_FIRST_RUN_EXPERIENCE = "disable-fre";

    // Force the First Run Experience to show.
    public static final String FORCE_FIRST_RUN_EXPERIENCE = "force-fre";

    // Default country code to be used for search engine localization.
    public static final String DEFAULT_COUNTRY_CODE_AT_INSTALL = "default-country-code";

    // Sets the max number of sandboxed service processes to use.
    // Unlike renderer-process-limit, this is a hard limit on the number of
    // concurrent sandboxed processes.
    public static final String SANDBOXED_SERVICE_LIMIT = "sandboxed-service-limit";

    // Mirrors of switches defined in native code.  It is up to you,
    // the developer, to keep these strings in sync with
    // base/base_switches.h, or content/public/common/content_switches.h, or
    // wherever.

    // Don't restore persistent state from saved files on startup.
    public static final String NO_RESTORE_STATE = "no-restore-state";

    // Specifies which page will be displayed on startup.
    public static final String HOME_PAGE = "homepage";

    // Dump frames-per-second to the log
    public static final String LOG_FPS = "log-fps";

    // Override the default server used for profile sync.
    public static final String SYNC_URL = "sync-url";

    // Sets the max number of render processes to use.
    public static final String RENDER_PROCESS_LIMIT = "renderer-process-limit";

    // Enable to swap the overview mode between different implementation.
    // TODO(jscholler): This should be removed eventually, as it is only used for development and
    // debugging.
    public static final String ENABLE_OVERVIEW_SWAP = "enable-overview-swap";

    // DO NOT ADD YOUR NEW SWITCH HERE unless it is a mirror of a native switch. Otherwise, see
    // the "Switches used from Java" section above.

    // Public abstract interface, implemented in derived classes.
    // All these methods reflect their native-side counterparts.
    /**
     *  Returns true if this command line contains the given switch.
     *  (Switch names ARE case-sensitive).
     */
    public abstract boolean hasSwitch(String switchString);

    /**
     * Return the value associated with the given switch, or null.
     * @param switchString The switch key to lookup. It should NOT start with '--' !
     * @return switch value, or null if the switch is not set or set to empty.
     */
    public abstract String getSwitchValue(String switchString);

    /**
     * Append a switch to the command line.  There is no guarantee
     * this action happens before the switch is needed.
     * @param switchString the switch to add.  It should NOT start with '--' !
     */
    public abstract void appendSwitch(String switchString);

    /**
     * Append a switch and value to the command line.  There is no
     * guarantee this action happens before the switch is needed.
     * @param switchString the switch to add.  It should NOT start with '--' !
     * @param value the value for this switch.
     * For example, --foo=bar becomes 'foo', 'bar'.
     */
    public abstract void appendSwitchWithValue(String switchString, String value);

    /**
     * Append switch/value items in "command line" format (excluding argv[0] program name).
     * E.g. { '--gofast', '--username=fred' }
     * @param array an array of switch or switch/value items in command line format.
     *   Unlike the other append routines, these switches SHOULD start with '--' .
     *   Unlike init(), this does not include the program name in array[0].
     */
    public abstract void appendSwitchesAndArguments(String[] array);

    /**
     * Determine if the command line is bound to the native (JNI) implementation.
     * @return true if the underlying implementation is delegating to the native command line.
     */
    public boolean isNativeImplementation() {
        return false;
    }

    private static final AtomicReference<CommandLine> sCommandLine =
        new AtomicReference<CommandLine>();

    /**
     * @returns true if the command line has already been initialized.
     */
    public static boolean isInitialized() {
        return sCommandLine.get() != null;
    }

    // Equivalent to CommandLine::ForCurrentProcess in C++.
    public static CommandLine getInstance() {
        assert sCommandLine.get() != null;
        return sCommandLine.get();
    }

    /**
     * Initialize the singleton instance, must be called exactly once (either directly or
     * via one of the convenience wrappers below) before using the static singleton instance.
     * @param args command line flags in 'argv' format: args[0] is the program name.
     */
    public static void init(String[] args) {
        assert sCommandLine.get() == null;
        sCommandLine.compareAndSet(null, new JavaCommandLine(args));
    }

    /**
     * Initialize the command line from the command-line file.
     *
     * @param file The fully qualified command line file.
     */
    public static void initFromFile(String file) {
        char[] buffer = new char[0];
        try {
            // Arbitrary clamp of 8k on the amount of file we read in.
            buffer = readUtf8FileFully(file, 8 * 1024);
        } catch (FileNotFoundException e) {
            // Ignore: having a command line file is optional.
        } catch (IOException e) {
            Log.w(TAG, "error reading command line file " + file + e);
        }
        init(tokenizeQuotedAruments(buffer));
    }

    /**
     * Resets both the java proxy and the native command lines. This allows the entire
     * command line initialization to be re-run including the call to onJniLoaded.
     */
    public static void reset() {
        if (sCommandLine.get() != null && sCommandLine.get().isNativeImplementation()) {
            nativeReset();
        }
        sCommandLine.set(null);
    }

    /**
     * Public for testing (TODO: why are the tests in a different package?)
     * Parse command line flags from a flat buffer, supporting double-quote enclosed strings
     * containing whitespace. argv elements are derived by splitting the buffer on whitepace;
     * double quote characters may enclose tokens containing whitespace; a double-quote literal
     * may be escaped with back-slash. (Otherwise backslash is taken as a literal).
     * @param buffer A command line in command line file format as described above.
     * @return the tokenized arguments, suitable for passing to init().
     */
    public static String[] tokenizeQuotedAruments(char[] buffer) {
        boolean inQuotes = false;
        ArrayList<String> args = new ArrayList<String>();
        StringBuilder arg = null;
        for (char c : buffer) {
            if (c == '\"') {
                if (arg != null && arg.length() > 0 && arg.charAt(arg.length() - 1) == '\\') {
                    // Last char was a backslash; pop it, and treat this " as a literal.
                    arg.setCharAt(arg.length() - 1, c);
                } else {
                    inQuotes = !inQuotes;
                }
            } else if (!inQuotes && Character.isWhitespace(c)) {
                if (arg != null) {
                    args.add(arg.toString());
                    arg = null;
                }
            } else {
                if (arg == null) arg = new StringBuilder();
                arg.append(c);
            }
        }
        if (arg != null) {
            if (inQuotes) {
                Log.w(TAG, "Unterminated quoted string: " + arg);
            }
            args.add(arg.toString());
        }
        return args.toArray(new String[args.size()]);
    }

    private static final String TAG = "CommandLine";
    private static final String SWITCH_PREFIX = "--";
    private static final String SWITCH_TERMINATOR = SWITCH_PREFIX;
    private static final String SWITCH_VALUE_SEPARATOR = "=";

    static void enableNativeProxy() {
        // Make a best-effort to ensure we make a clean (atomic) switch over from the old to
        // the new command line implementation. If another thread is modifying the command line
        // when this happens, all bets are off. (As per the native CommandLine).
        sCommandLine.set(new NativeCommandLine());
    }

    static String[] getJavaSwitchesOrNull() {
        CommandLine commandLine = sCommandLine.get();
        if (commandLine != null) {
            assert !commandLine.isNativeImplementation();
            return ((JavaCommandLine) commandLine).getCommandLineArguments();
        }
        return null;
    }

    /**
     * @param fileName the file to read in.
     * @param sizeLimit cap on the file size: will throw an exception if exceeded
     * @return Array of chars read from the file
     * @throws FileNotFoundException file does not exceed
     * @throws IOException error encountered accessing the file
     */
    private static char[] readUtf8FileFully(String fileName, int sizeLimit) throws
            FileNotFoundException, IOException {
        Reader reader = null;
        try {
            File f = new File(fileName);
            if (f.length() > sizeLimit) {
                throw new IOException("File " + fileName + " length " + f.length() +
                        " exceeds limit " + sizeLimit);
            }
            char[] buffer = new char[(int) f.length()];
            reader = new InputStreamReader(new FileInputStream(f), "UTF-8");
            int charsRead = reader.read(buffer);
            // Debug check that we've exhausted the input stream (will fail e.g. if the
            // file grew after we inspected its length).
            assert !reader.ready();
            return charsRead < buffer.length ? Arrays.copyOfRange(buffer, 0, charsRead) : buffer;
        } finally {
            if (reader != null) reader.close();
        }
    }

    private CommandLine() {}

    private static class JavaCommandLine extends CommandLine {
        private HashMap<String, String> mSwitches = new HashMap<String, String>();
        private ArrayList<String> mArgs = new ArrayList<String>();

        // The arguments begin at index 1, since index 0 contains the executable name.
        private int mArgsBegin = 1;

        JavaCommandLine(String[] args) {
            if (args == null || args.length == 0 || args[0] == null) {
                mArgs.add("");
            } else {
                mArgs.add(args[0]);
                appendSwitchesInternal(args, 1);
            }
            // Invariant: we always have the argv[0] program name element.
            assert mArgs.size() > 0;
        }

        /**
         * Returns the switches and arguments passed into the program, with switches and their
         * values coming before all of the arguments.
         */
        private String[] getCommandLineArguments() {
            return mArgs.toArray(new String[mArgs.size()]);
        }

        @Override
        public boolean hasSwitch(String switchString) {
            return mSwitches.containsKey(switchString);
        }

        @Override
        public String getSwitchValue(String switchString) {
            // This is slightly round about, but needed for consistency with the NativeCommandLine
            // version which does not distinguish empty values from key not present.
            String value = mSwitches.get(switchString);
            return value == null || value.isEmpty() ? null : value;
        }

        @Override
        public void appendSwitch(String switchString) {
            appendSwitchWithValue(switchString, null);
        }

        /**
         * Appends a switch to the current list.
         * @param switchString the switch to add.  It should NOT start with '--' !
         * @param value the value for this switch.
         */
        @Override
        public void appendSwitchWithValue(String switchString, String value) {
            mSwitches.put(switchString, value == null ? "" : value);

            // Append the switch and update the switches/arguments divider mArgsBegin.
            String combinedSwitchString = SWITCH_PREFIX + switchString;
            if (value != null && !value.isEmpty())
                combinedSwitchString += SWITCH_VALUE_SEPARATOR + value;

            mArgs.add(mArgsBegin++, combinedSwitchString);
        }

        @Override
        public void appendSwitchesAndArguments(String[] array) {
            appendSwitchesInternal(array, 0);
        }

        // Add the specified arguments, but skipping the first |skipCount| elements.
        private void appendSwitchesInternal(String[] array, int skipCount) {
            boolean parseSwitches = true;
            for (String arg : array) {
                if (skipCount > 0) {
                    --skipCount;
                    continue;
                }

                if (arg.equals(SWITCH_TERMINATOR)) {
                    parseSwitches = false;
                }

                if (parseSwitches && arg.startsWith(SWITCH_PREFIX)) {
                    String[] parts = arg.split(SWITCH_VALUE_SEPARATOR, 2);
                    String value = parts.length > 1 ? parts[1] : null;
                    appendSwitchWithValue(parts[0].substring(SWITCH_PREFIX.length()), value);
                } else {
                    mArgs.add(arg);
                }
            }
        }
    }

    private static class NativeCommandLine extends CommandLine {
        @Override
        public boolean hasSwitch(String switchString) {
            return nativeHasSwitch(switchString);
        }

        @Override
        public String getSwitchValue(String switchString) {
            return nativeGetSwitchValue(switchString);
        }

        @Override
        public void appendSwitch(String switchString) {
            nativeAppendSwitch(switchString);
        }

        @Override
        public void appendSwitchWithValue(String switchString, String value) {
            nativeAppendSwitchWithValue(switchString, value);
        }

        @Override
        public void appendSwitchesAndArguments(String[] array) {
            nativeAppendSwitchesAndArguments(array);
        }

        @Override
        public boolean isNativeImplementation() {
            return true;
        }
    }

    private static native void nativeReset();
    private static native boolean nativeHasSwitch(String switchString);
    private static native String nativeGetSwitchValue(String switchString);
    private static native void nativeAppendSwitch(String switchString);
    private static native void nativeAppendSwitchWithValue(String switchString, String value);
    private static native void nativeAppendSwitchesAndArguments(String[] array);
};
