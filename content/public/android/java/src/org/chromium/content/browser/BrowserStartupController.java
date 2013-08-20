// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Handler;
import android.util.Log;

import com.google.common.annotations.VisibleForTesting;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;
import org.chromium.content.common.ProcessInitException;

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

    public interface StartupCallback {
        void onSuccess(boolean alreadyStarted);
        void onFailure();
    }

    private static final String TAG = "BrowserStartupController";

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

    private static void setAsynchronousStartupConfig() {
        sBrowserMayStartAsynchronously = true;
    }

    @CalledByNative
    private static boolean browserMayStartAsynchonously() {
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
    private Context mContext;

    // Whether the async startup of the browser process has started.
    private boolean mHasStartedInitializingBrowserProcess;

    // Whether the async startup of the browser process is complete.
    private boolean mAsyncStartupDone;

    // This field is set after startup has been completed based on whether the startup was a success
    // or not. It is used when later requests to startup come in that happen after the initial set
    // of enqueued callbacks have been executed.
    private boolean mStartupSuccess;

    BrowserStartupController(Context context) {
        mContext = context;
        mAsyncStartupCallbacks = new ArrayList<StartupCallback>();
    }

    public static BrowserStartupController get(Context context) {
        assert ThreadUtils.runningOnUiThread() : "Tried to start the browser on the wrong thread.";
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new BrowserStartupController(context.getApplicationContext());
        }
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
    public void startBrowserProcessesAsync(final StartupCallback callback) {
        assert ThreadUtils.runningOnUiThread() : "Tried to start the browser on the wrong thread.";
        if (mAsyncStartupDone) {
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

            enableAsynchronousStartup();

            // Try to initialize the Android browser process.
            tryToInitializeBrowserProcess();
        }
    }

    private void tryToInitializeBrowserProcess() {
        try {
            assert mContext != null;
            boolean wasAlreadyInitialized = initializeAndroidBrowserProcess();
            // The context is not needed anymore, so clear the member field to not leak.
            mContext = null;
            if (wasAlreadyInitialized) {
                // Something has already initialized the browser process before we got to setup the
                // async startup. This means that we will never get a callback, so manually call
                // them now, and just assume that the startup was successful.
                Log.w(TAG, "Browser process was initialized without BrowserStartupController");
                enqueueCallbackExecution(STARTUP_SUCCESS, ALREADY_STARTED);
            }
        } catch (ProcessInitException e) {
            Log.e(TAG, "Unable to start browser process.", e);
            // ProcessInitException could mean one of two things:
            // 1) The LibraryLoader failed.
            // 2) ContentMain failed to start.
            // It is unclear whether the browser tasks have already been started, and in case they
            // have not, post a message to execute all the callbacks. Whichever call to
            // executeEnqueuedCallbacks comes first will trigger the callbacks, but since the list
            // of callbacks is then cleared, they will only be called once.
            enqueueCallbackExecution(STARTUP_FAILURE, NOT_ALREADY_STARTED);
        }
    }

    public void addStartupCompletedObserver(StartupCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (mAsyncStartupDone)
            postStartupCompleted(callback);
        else
            mAsyncStartupCallbacks.add(callback);
    }

    private void executeEnqueuedCallbacks(int startupResult, boolean alreadyStarted) {
        assert ThreadUtils.runningOnUiThread() : "Callback from browser startup from wrong thread.";
        mAsyncStartupDone = true;
        for (StartupCallback asyncStartupCallback : mAsyncStartupCallbacks) {
            if (startupResult > 0) {
                asyncStartupCallback.onFailure();
            } else {
                mStartupSuccess = true;
                asyncStartupCallback.onSuccess(alreadyStarted);
            }
        }
        // We don't want to hold on to any objects after we do not need them anymore.
        mAsyncStartupCallbacks.clear();
    }

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
                if (mStartupSuccess)
                    callback.onSuccess(ALREADY_STARTED);
                else
                    callback.onFailure();
            }
        });
    }

    /**
     * Ensure that the browser process will be asynchronously started up. This also ensures that we
     * get a call to {@link #browserStartupComplete} when the browser startup is complete.
     */
    @VisibleForTesting
    void enableAsynchronousStartup() {
        setAsynchronousStartupConfig();
    }

    /**
     * @return whether the process was already initialized, so native was not instructed to start.
     */
    @VisibleForTesting
    boolean initializeAndroidBrowserProcess() throws ProcessInitException {
        return !AndroidBrowserProcess.init(mContext, AndroidBrowserProcess.MAX_RENDERERS_LIMIT);
    }
}
