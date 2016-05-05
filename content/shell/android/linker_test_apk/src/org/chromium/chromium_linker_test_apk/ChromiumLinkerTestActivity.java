// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromium_linker_test_apk;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.Linker;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_shell.Shell;
import org.chromium.content_shell.ShellManager;
import org.chromium.ui.base.ActivityWindowAndroid;

/**
 * Test activity used for verifying the different configuration options for the ContentLinker.
 */
public class ChromiumLinkerTestActivity extends Activity {
    private static final String TAG = "LinkerTest";

    public static final String COMMAND_LINE_FILE =
            "/data/local/tmp/chromium-linker-test-command-line";

    public static final String COMMAND_LINE_ARGS_KEY = "commandLineArgs";

    // Use this on the command-line to simulate a low-memory device, otherwise
    // a regular device is simulated by this test, independently from what the
    // target device running the test really is.
    private static final String LOW_MEMORY_DEVICE = "--low-memory-device";

    // Use one of these on the command-line to force a specific Linker
    // implementation. Passed from the main process to sub-processes so that
    // everything that participates in a test uses a consistent implementation.
    private static final String USE_MODERN_LINKER = "--use-linker=modern";
    private static final String USE_LEGACY_LINKER = "--use-linker=legacy";

    private ShellManager mShellManager;
    private ActivityWindowAndroid mWindowAndroid;

    @SuppressFBWarnings("DMI_HARDCODED_ABSOLUTE_FILENAME")
    @Override
    public void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Initializing the command line must occur before loading the library.
        if (!CommandLine.isInitialized()) {
            CommandLine.initFromFile(COMMAND_LINE_FILE);
            String[] commandLineParams = getCommandLineParamsFromIntent(getIntent());
            if (commandLineParams != null) {
                CommandLine.getInstance().appendSwitchesAndArguments(commandLineParams);
            }
        }
        waitForDebuggerIfNeeded();

        // CommandLine.getInstance().hasSwitch() doesn't work here for some funky
        // reason, so parse the command-line differently here:
        boolean hasLowMemoryDeviceSwitch = false;
        boolean hasModernLinkerSwitch = false;
        boolean hasLegacyLinkerSwitch = false;
        String[] commandLine = CommandLine.getJavaSwitchesOrNull();
        if (commandLine == null) {
            Log.i(TAG, "Command line is null");
        } else {
            Log.i(TAG, "Command line is:");
            for (int n = 0; n < commandLine.length; ++n) {
                String option = commandLine[n];
                Log.i(TAG, "  '" + option + "'");
                if (option.equals(LOW_MEMORY_DEVICE)) {
                    hasLowMemoryDeviceSwitch = true;
                } else if (option.equals(USE_MODERN_LINKER)) {
                    hasModernLinkerSwitch = true;
                } else if (option.equals(USE_LEGACY_LINKER)) {
                    hasLegacyLinkerSwitch = true;
                }
            }
        }

        if (!(hasModernLinkerSwitch || hasLegacyLinkerSwitch)) {
            Log.e(TAG, "Missing --use-linker command line argument.");
            finish();
        } else if (hasModernLinkerSwitch && hasLegacyLinkerSwitch) {
            Log.e(TAG, "Conflicting --use-linker command line arguments.");
            finish();
        }

        // Set the requested Linker implementation from the command-line, and
        // register the test runner class by name.
        if (hasModernLinkerSwitch) {
            Linker.setupForTesting(Linker.LINKER_IMPLEMENTATION_MODERN,
                                   LinkerTests.class.getName());
        } else {
            Linker.setupForTesting(Linker.LINKER_IMPLEMENTATION_LEGACY,
                                   LinkerTests.class.getName());
        }

        // Determine which kind of device to simulate from the command-line.
        int memoryDeviceConfig = Linker.MEMORY_DEVICE_CONFIG_NORMAL;
        if (hasLowMemoryDeviceSwitch) {
            memoryDeviceConfig = Linker.MEMORY_DEVICE_CONFIG_LOW;
        }
        Linker linker = Linker.getInstance();
        linker.setMemoryDeviceConfigForTesting(memoryDeviceConfig);

        // Load the library in the browser process, this will also run the test
        // runner in this process.
        try {
            LibraryLoader.get(LibraryProcessType.PROCESS_BROWSER)
                    .ensureInitialized(getApplicationContext());
        } catch (ProcessInitException e) {
            Log.i(TAG, "Cannot load chromium_linker_test:" +  e);
        }

        // Now, start a new renderer process by creating a new view.
        // This will run the test runner in the renderer process.

        BrowserStartupController.get(getApplicationContext(), LibraryProcessType.PROCESS_BROWSER)
                .initChromiumBrowserProcessForTests();

        LayoutInflater inflater =
                (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View view = inflater.inflate(R.layout.test_activity, null);
        mShellManager = (ShellManager) view.findViewById(R.id.shell_container);
        mWindowAndroid = new ActivityWindowAndroid(this);
        mShellManager.setWindow(mWindowAndroid);

        mShellManager.setStartupUrl("about:blank");

        try {
            BrowserStartupController.get(this, LibraryProcessType.PROCESS_BROWSER)
                    .startBrowserProcessesAsync(
                            true,
                            new BrowserStartupController.StartupCallback() {
                                @Override
                                public void onSuccess(boolean alreadyStarted) {
                                    finishInitialization(savedInstanceState);
                                }

                                @Override
                                public void onFailure() {
                                    initializationFailed();
                                }
                            });
        } catch (ProcessInitException e) {
            Log.e(TAG, "Unable to load native library.", e);
            finish();
        }

        // TODO(digit): Ensure that after the content view is initialized,
        // the program finishes().
    }

    private void finishInitialization(Bundle savedInstanceState) {
        String shellUrl = ShellManager.DEFAULT_SHELL_URL;
        mShellManager.launchShell(shellUrl);
        getActiveContentViewCore().setContentViewClient(new ContentViewClient());
    }

    private void initializationFailed() {
        Log.e(TAG, "ContentView initialization failed.");
        finish();
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        mWindowAndroid.saveInstanceState(outState);
    }

    private void waitForDebuggerIfNeeded() {
        if (CommandLine.getInstance().hasSwitch(BaseSwitches.WAIT_FOR_JAVA_DEBUGGER)) {
            Log.e(TAG, "Waiting for Java debugger to connect...");
            android.os.Debug.waitForDebugger();
            Log.e(TAG, "Java debugger connected. Resuming execution.");
        }
    }

    @Override
    protected void onStop() {
        super.onStop();

        ContentViewCore contentViewCore = getActiveContentViewCore();
        if (contentViewCore != null) contentViewCore.onHide();
    }

    @Override
    protected void onStart() {
        super.onStart();

        ContentViewCore contentViewCore = getActiveContentViewCore();
        if (contentViewCore != null) contentViewCore.onShow();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        mWindowAndroid.onActivityResult(requestCode, resultCode, data);
    }

    private static String[] getCommandLineParamsFromIntent(Intent intent) {
        return intent != null ? intent.getStringArrayExtra(COMMAND_LINE_ARGS_KEY) : null;
    }

    /**
     * @return The {@link ContentViewCore} owned by the currently visible {@link Shell} or null if
     *         one is not showing.
     */
    public ContentViewCore getActiveContentViewCore() {
        if (mShellManager == null) {
            return null;
        }

        Shell shell = mShellManager.getActiveShell();
        if (shell == null) {
            return null;
        }

        return shell.getContentViewCore();
    }
}
