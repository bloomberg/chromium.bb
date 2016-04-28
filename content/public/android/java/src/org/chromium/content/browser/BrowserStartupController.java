// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Handler;

import org.chromium.base.Log;
import org.chromium.base.ResourceExtractor;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.app.ContentMain;

import java.util.ArrayList;
import java.util.List;

/**
 * This class controls how C++ browser main loop is started and ensures it happens only once.
 *
 * It supports kicking off the startup sequence in an asynchronous way. Startup can be called as
 * many times as needed (for instance, multiple activities for the same application), but the
 * browser process will still only be initialized once. All requests to start the browser will
 * always get their callback executed; if the browser process has already been started, the callback
 * is called immediately, else it is called when initialization is complete.
 *
 * All communication with this class must happen on the main thread.
 *
 * This is a singleton, and stores a reference to the application context.
 */
@JNINamespace("content")
public class BrowserStartupController {

    /**
     * This provides the interface to the callbacks for successful or failed startup
     */
    public interface StartupCallback {
        void onSuccess(boolean alreadyStarted);
        void onFailure();
    }

    private static final String TAG = "cr.BrowserStartup";

    // Helper constants for {@link StartupCallback#onSuccess}.
    private static final boolean ALREADY_STARTED = true;
    private static final boolean NOT_ALREADY_STARTED = false;

    // Helper constants for {@link #executeEnqueuedCallbacks(int, boolean)}.
    @VisibleForTesting
    static final int STARTUP_SUCCESS = -1;
    @VisibleForTesting
    static final int STARTUP_FAILURE = 1;

    private static BrowserStartupController sInstance;

    private static boolean sBrowserMayStartAsynchronously = false;

    private static void setAsynchronousStartup(boolean enable) {
        sBrowserMayStartAsynchronously = enable;
    }

    @VisibleForTesting
    @CalledByNative
    static boolean browserMayStartAsynchonously() {
        return sBrowserMayStartAsynchronously;
    }

    @VisibleForTesting
    @CalledByNative
    static void browserStartupComplete(int result) {
        if (sInstance != null) {
            sInstance.executeEnqueuedCallbacks(result, NOT_ALREADY_STARTED);
        }
    }

    // A list of callbacks that should be called when the async startup of the browser process is
    // complete.
    private final List<StartupCallback> mAsyncStartupCallbacks;

    // The context is set on creation, but the reference is cleared after the browser process
    // initialization has been started, since it is not needed anymore. This is to ensure the
    // context is not leaked.
    private final Context mContext;

    // Whether the async startup of the browser process has started.
    private boolean mHasStartedInitializingBrowserProcess;

    // Whether tasks that occur after resource extraction have been completed.
    private boolean mPostResourceExtractionTasksCompleted;

    // Whether the async startup of the browser process is complete.
    private boolean mStartupDone;

    // This field is set after startup has been completed based on whether the startup was a success
    // or not. It is used when later requests to startup come in that happen after the initial set
    // of enqueued callbacks have been executed.
    private boolean mStartupSuccess;

    private int mLibraryProcessType;

    BrowserStartupController(Context context, int libraryProcessType) {
        mContext = context.getApplicationContext();
        mAsyncStartupCallbacks = new ArrayList<StartupCallback>();
        mLibraryProcessType = libraryProcessType;
    }

    /**
     * Get BrowserStartupController instance, create a new one if no existing.
     *
     * @param context the application context.
     * @param libraryProcessType the type of process the shared library is loaded. it must be
     *                           LibraryProcessType.PROCESS_BROWSER or
     *                           LibraryProcessType.PROCESS_WEBVIEW.
     * @return BrowserStartupController instance.
     */
    public static BrowserStartupController get(Context context, int libraryProcessType) {
        assert ThreadUtils.runningOnUiThread() : "Tried to start the browser on the wrong thread.";
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            assert LibraryProcessType.PROCESS_BROWSER == libraryProcessType
                    || LibraryProcessType.PROCESS_WEBVIEW == libraryProcessType;
            sInstance = new BrowserStartupController(context, libraryProcessType);
        }
        assert sInstance.mLibraryProcessType == libraryProcessType : "Wrong process type";
        return sInstance;
    }

    @VisibleForTesting
    static BrowserStartupController overrideInstanceForTest(BrowserStartupController controller) {
        if (sInstance == null) {
            sInstance = controller;
        }
        return sInstance;
    }

    /**
     * Start the browser process asynchronously. This will set up a queue of UI thread tasks to
     * initialize the browser process.
     * <p/>
     * Note that this can only be called on the UI thread.
     *
     * @param callback the callback to be called when browser startup is complete.
     */
    public void startBrowserProcessesAsync(final StartupCallback callback)
            throws ProcessInitException {
        assert ThreadUtils.runningOnUiThread() : "Tried to start the browser on the wrong thread.";
        if (mStartupDone) {
            // Browser process initialization has already been completed, so we can immediately post
            // the callback.
            postStartupCompleted(callback);
            return;
        }

        // Browser process has not been fully started yet, so we defer executing the callback.
        mAsyncStartupCallbacks.add(callback);

        if (!mHasStartedInitializingBrowserProcess) {
            // This is the first time we have been asked to start the browser process. We set the
            // flag that indicates that we have kicked off starting the browser process.
            mHasStartedInitializingBrowserProcess = true;

            setAsynchronousStartup(true);
            prepareToStartBrowserProcess(false, new Runnable() {
                @Override
                public void run() {
                    ThreadUtils.assertOnUiThread();
                    if (contentStart() > 0) {
                        // Failed. The callbacks may not have run, so run them.
                        enqueueCallbackExecution(STARTUP_FAILURE, NOT_ALREADY_STARTED);
                    }
                }
            });
        }
    }

    /**
     * Start the browser process synchronously. If the browser is already being started
     * asynchronously then complete startup synchronously
     *
     * <p/>
     * Note that this can only be called on the UI thread.
     *
     * @param singleProcess true iff the browser should run single-process, ie. keep renderers in
     *                      the browser process
     * @throws ProcessInitException
     */
    public void startBrowserProcessesSync(boolean singleProcess) throws ProcessInitException {
        // If already started skip to checking the result
        if (!mStartupDone) {
            if (!mHasStartedInitializingBrowserProcess || !mPostResourceExtractionTasksCompleted) {
                prepareToStartBrowserProcess(singleProcess, null);
            }

            setAsynchronousStartup(false);
            if (contentStart() > 0) {
                // Failed. The callbacks may not have run, so run them.
                enqueueCallbackExecution(STARTUP_FAILURE, NOT_ALREADY_STARTED);
            }
        }

        // Startup should now be complete
        assert mStartupDone;
        if (!mStartupSuccess) {
            throw new ProcessInitException(LoaderErrors.LOADER_ERROR_NATIVE_STARTUP_FAILED);
        }
    }

    /**
     * Wrap ContentMain.start() for testing.
     */
    @VisibleForTesting
    int contentStart() {
        return ContentMain.start();
    }

    public void addStartupCompletedObserver(StartupCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (mStartupDone) {
            postStartupCompleted(callback);
        } else {
            mAsyncStartupCallbacks.add(callback);
        }
    }

    private void executeEnqueuedCallbacks(int startupResult, boolean alreadyStarted) {
        assert ThreadUtils.runningOnUiThread() : "Callback from browser startup from wrong thread.";
        mStartupDone = true;
        mStartupSuccess = (startupResult <= 0);
        for (StartupCallback asyncStartupCallback : mAsyncStartupCallbacks) {
            if (mStartupSuccess) {
                asyncStartupCallback.onSuccess(alreadyStarted);
            } else {
                asyncStartupCallback.onFailure();
            }
        }
        // We don't want to hold on to any objects after we do not need them anymore.
        mAsyncStartupCallbacks.clear();
    }

    // Queue the callbacks to run. Since running the callbacks clears the list it is safe to call
    // this more than once.
    private void enqueueCallbackExecution(final int startupFailure, final boolean alreadyStarted) {
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                executeEnqueuedCallbacks(startupFailure, alreadyStarted);
            }
        });
    }

    private void postStartupCompleted(final StartupCallback callback) {
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                if (mStartupSuccess) {
                    callback.onSuccess(ALREADY_STARTED);
                } else {
                    callback.onFailure();
                }
            }
        });
    }

    @VisibleForTesting
    void prepareToStartBrowserProcess(
            final boolean singleProcess, final Runnable completionCallback)
                    throws ProcessInitException {
        Log.i(TAG, "Initializing chromium process, singleProcess=%b", singleProcess);

        // Normally Main.java will have kicked this off asynchronously for Chrome. But other
        // ContentView apps like tests also need them so we make sure we've extracted resources
        // here. We can still make it a little async (wait until the library is loaded).
        ResourceExtractor resourceExtractor = ResourceExtractor.get(mContext);
        resourceExtractor.startExtractingResources();

        // Normally Main.java will have already loaded the library asynchronously, we only need
        // to load it here if we arrived via another flow, e.g. bookmark access & sync setup.
        LibraryLoader.get(mLibraryProcessType).ensureInitialized(mContext);

        Runnable postResourceExtraction = new Runnable() {
            @Override
            public void run() {
                if (!mPostResourceExtractionTasksCompleted) {
                    // TODO(yfriedman): Remove dependency on a command line flag for this.
                    DeviceUtils.addDeviceSpecificUserAgentSwitch(mContext);

                    nativeSetCommandLineFlags(
                            singleProcess, nativeIsPluginEnabled() ? getPlugins() : null);
                    mPostResourceExtractionTasksCompleted = true;
                }

                if (completionCallback != null) completionCallback.run();
            }
        };

        if (completionCallback == null) {
            // If no continuation callback is specified, then force the resource extraction
            // to complete.
            resourceExtractor.waitForCompletion();
            postResourceExtraction.run();
        } else {
            resourceExtractor.addCompletionCallback(postResourceExtraction);
        }
    }

    /**
     * Initialization needed for tests. Mainly used by content browsertests.
     */
    public void initChromiumBrowserProcessForTests() {
        ResourceExtractor resourceExtractor = ResourceExtractor.get(mContext);
        resourceExtractor.startExtractingResources();
        resourceExtractor.waitForCompletion();
        nativeSetCommandLineFlags(false, null);
    }

    private String getPlugins() {
        return PepperPluginManager.getPlugins(mContext);
    }

    private static native void nativeSetCommandLineFlags(
            boolean singleProcess, String pluginDescriptor);

    // Is this an official build of Chrome? Only native code knows for sure. Official build
    // knowledge is needed very early in process startup.
    private static native boolean nativeIsOfficialBuild();

    private static native boolean nativeIsPluginEnabled();
}
