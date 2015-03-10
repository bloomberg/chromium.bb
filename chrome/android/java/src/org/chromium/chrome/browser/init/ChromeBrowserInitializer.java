// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.StrictMode;
import android.preference.PreferenceManager;
import android.util.Log;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ResourceExtractor;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.ChromeSwitches;
import org.chromium.chrome.browser.ChromiumApplication;
import org.chromium.chrome.browser.FileProviderHelper;
import org.chromium.content.app.ContentApplication;
import org.chromium.content.browser.BrowserStartupController;
import org.chromium.content.browser.DeviceUtils;
import org.chromium.content.browser.SpeechRecognition;
import org.chromium.net.NetworkChangeNotifier;

import java.util.LinkedList;

/**
 * Application level delegate that handles start up tasks.
 * {@link AsyncInitializationActivity} classes should override the {@link BrowserParts}
 * interface for any additional initialization tasks for the initialization to work as intended.
 */
public class ChromeBrowserInitializer {
    private static final String TAG = "ChromeBrowserInitializer";
    private static ChromeBrowserInitializer sChromeBrowserInitiliazer;

    private final Handler mHandler;
    private final ChromiumApplication mApplication;
    private boolean mPreInflationStartupComplete;
    private boolean mPostInflationStartupComplete;
    private boolean mNativeInitializationComplete;

    /**
     * A callback to be executed when there is a new version available in Play Store.
     */
    public interface OnNewVersionAvailableCallback extends Runnable {
        /**
         * Set the update url to get the new version available.
         * @param updateUrl The url to be used.
         */
        void setUpdateUrl(String updateUrl);
    }

    /**
     * This class is an application specific object that orchestrates the app initialization.
     * @param context The context to get the application context from.
     * @return The singleton instance of {@link ChromeBrowserInitializer}.
     */
    public static ChromeBrowserInitializer getInstance(Context context) {
        if (sChromeBrowserInitiliazer == null) {
            sChromeBrowserInitiliazer = new ChromeBrowserInitializer(context);
        }
        return sChromeBrowserInitiliazer;
    }

    private ChromeBrowserInitializer(Context context) {
        mApplication = (ChromiumApplication) context.getApplicationContext();
        mHandler = new Handler(Looper.getMainLooper());
    }

    /**
     * Execute startup tasks that can be done without native libraries. See {@link BrowserParts} for
     * a list of calls to be implemented.
     * @param parts The delegate for the {@link ChromeBrowserInitializer} to communicate
     *              initialization tasks.
     */
    public void handlePreNativeStartup(final BrowserParts parts) {
        preInflationStartup();
        parts.preInflationStartup();
        parts.setContentViewAndLoadLibrary();
        postInflationStartup();
        parts.postInflationStartup();
    }

    private void preInflationStartup() {
        ThreadUtils.assertOnUiThread();
        if (mPreInflationStartupComplete) return;

        // Ensure critical files are available, so they aren't blocked on the file-system
        // behind long-running accesses in next phase.
        // Don't do any large file access here!
        ContentApplication.initCommandLine(mApplication);
        waitForDebuggerIfNeeded();
        configureStrictMode();

        // Warm up the shared prefs stored.
        PreferenceManager.getDefaultSharedPreferences(mApplication);

        DeviceUtils.addDeviceSpecificUserAgentSwitch(mApplication);

        mPreInflationStartupComplete = true;
    }

    private void postInflationStartup() {
        ThreadUtils.assertOnUiThread();
        if (mPostInflationStartupComplete) return;

        // Check to see if we need to extract any new resources from the APK. This could
        // be on first run when we need to extract all the .pak files we need, or after
        // the user has switched locale, in which case we want new locale resources.
        ResourceExtractor.get(mApplication).startExtractingResources();

        mPostInflationStartupComplete = true;
    }

    /**
     * Execute startup tasks that require native libraries to be loaded. See {@link BrowserParts}
     * for a list of calls to be implemented.
     * @param parts The delegate for the {@link ChromeBrowserInitializer} to communicate
     *              initialization tasks.
     */
    public void handlePostNativeStartup(final BrowserParts delegate) {
        final LinkedList<Runnable> initQueue = new LinkedList<Runnable>();

        abstract class NativeInitTask implements Runnable {
            @Override
            public final void run() {
                // Run the current task then put a request for the next one onto the
                // back of the UI message queue. This lets Chrome handle input events
                // between tasks.
                initFunction();
                if (!initQueue.isEmpty()) {
                    mHandler.post(initQueue.pop());
                }
            }
            public abstract void initFunction();
        }

        initQueue.add(new NativeInitTask() {
            @Override
            public void initFunction() {
                mApplication.initializeProcess();
            }
        });

        initQueue.add(new NativeInitTask() {
            @Override
            public void initFunction() {
                onStartNativeInitialization();
            }
        });

        initQueue.add(new NativeInitTask() {
            @Override
            public void initFunction() {
                if (delegate.isActivityDestroyed()) return;
                delegate.initializeCompositor();
            }
        });

        initQueue.add(new NativeInitTask() {
            @Override
            public void initFunction() {
                if (delegate.isActivityDestroyed()) return;
                delegate.initializeState();
            }
        });

        initQueue.add(new NativeInitTask() {
            @Override
            public void initFunction() {
                onFinishNativeInitialization();
            }
        });

        initQueue.add(new NativeInitTask() {
            @Override
            public void initFunction() {
                if (delegate.isActivityDestroyed()) return;
                delegate.finishNativeInitialization();
            }
        });

        // We want to start this queue once the C++ startup tasks have run; allow the
        // C++ startup to run asynchonously, and set it up to start the Java queue once
        // it has finished.
        startChromeBrowserProcesses(delegate, new BrowserStartupController.StartupCallback() {
            @Override
            public void onFailure() {
                delegate.onStartupFailure();
            }

            @Override
            public void onSuccess(boolean arg0) {
                mHandler.post(initQueue.pop());
            }
        });
    }

    private void startChromeBrowserProcesses(BrowserParts parts,
            BrowserStartupController.StartupCallback callback) {
        try {
            TraceEvent.begin("ChromeBrowserInitializer.startChromeBrowserProcesses");
            mApplication.startChromeBrowserProcessesAsync(callback);
        } catch (ProcessInitException e) {
            parts.onStartupFailure();
        } finally {
            TraceEvent.end("ChromeBrowserInitializer.startChromeBrowserProcesses");
        }
    }

    private void onStartNativeInitialization() {
        ThreadUtils.assertOnUiThread();
        if (mNativeInitializationComplete) return;

        SpeechRecognition.initialize(mApplication);

        // Enable auto-detection of network connectivity state changes.
        NetworkChangeNotifier.init(mApplication);
        NetworkChangeNotifier.setAutoDetectConnectivityState(true);
    }

    private void onFinishNativeInitialization() {
        if (mNativeInitializationComplete) return;

        mNativeInitializationComplete = true;
        ContentUriUtils.setFileProviderUtil(new FileProviderHelper());
    }

    private static void configureStrictMode() {
        CommandLine commandLine = CommandLine.getInstance();
        if ("eng".equals(Build.TYPE) || commandLine.hasSwitch(ChromeSwitches.STRICT_MODE)) {
            StrictMode.enableDefaults();
            StrictMode.ThreadPolicy.Builder policy =
                    new StrictMode.ThreadPolicy.Builder(StrictMode.getThreadPolicy());
            policy = policy.detectAll()
                    .penaltyFlashScreen()
                    .penaltyDeathOnNetwork();
            if ("death".equals(commandLine.getSwitchValue(ChromeSwitches.STRICT_MODE))) {
                policy = policy.penaltyDeath();
                StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder(StrictMode.getVmPolicy())
                        .penaltyDeath()
                        .build());
            }
            StrictMode.setThreadPolicy(policy.build());
        }
    }

    private void waitForDebuggerIfNeeded() {
        if (CommandLine.getInstance().hasSwitch(BaseSwitches.WAIT_FOR_JAVA_DEBUGGER)) {
            Log.e(TAG, "Waiting for Java debugger to connect...");
            android.os.Debug.waitForDebugger();
            Log.e(TAG, "Java debugger connected. Resuming execution.");
        }
    }
}
