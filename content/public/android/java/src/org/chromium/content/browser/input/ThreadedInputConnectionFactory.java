// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.os.Handler;
import android.os.HandlerThread;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;

/**
 * A factory class for {@link ThreadedInputConnection}. The class also includes triggering
 * mechanism (hack) to run our InputConnection on non-UI thread.
 */
// TODO(changwan): add unit tests once Robolectric supports Android API level >= 21.
// See crbug.com/588547 for details.
public class ThreadedInputConnectionFactory implements ChromiumBaseInputConnection.Factory {
    private static final String TAG = "cr_Ime";
    private static final boolean DEBUG_LOGS = false;

    private final Handler mHandler;
    private final InputMethodManagerWrapper mInputMethodManagerWrapper;
    private final InputMethodUma mInputMethodUma;
    private ThreadedInputConnectionProxyView mProxyView;
    private ThreadedInputConnection mThreadedInputConnection;

    ThreadedInputConnectionFactory(
            InputMethodManagerWrapper inputMethodManagerWrapper) {
        mInputMethodManagerWrapper = inputMethodManagerWrapper;
        mHandler = createHandler();
        mInputMethodUma = createInputMethodUma();
    }

    @VisibleForTesting
    protected Handler createHandler() {
        HandlerThread thread =
                new HandlerThread("InputConnectionHandlerThread", HandlerThread.NORM_PRIORITY);
        thread.start();
        return new Handler(thread.getLooper());
    }

    @VisibleForTesting
    protected ThreadedInputConnectionProxyView createProxyView(
            Handler handler, View containerView) {
        return new ThreadedInputConnectionProxyView(
                containerView.getContext(), handler, containerView);
    }

    @VisibleForTesting
    protected InputMethodUma createInputMethodUma() {
        return new InputMethodUma();
    }

    private boolean shouldTriggerDelayedOnCreateInputConnection() {
        // Note that ThreadedInputConnectionProxyView intentionally calls
        // View#onCreateInputConnection() and not a separate method in this class.
        // There are third party apps that override WebView#onCreateInputConnection(),
        // and we still want to call them for consistency. The setback here is that the only
        // way to distinguish calls from InputMethodManager and from ProxyView is by looking at
        // the call stack.
        for (StackTraceElement ste : Thread.currentThread().getStackTrace()) {
            String className = ste.getClassName();
            if (className != null
                    && (className.contains(ThreadedInputConnectionProxyView.class.getName())
                    || className.contains("TestInputMethodManagerWrapper"))) {
                return false;
            }
        }
        return true;
    }

    @Override
    public ThreadedInputConnection initializeAndGet(
            View view, ImeAdapter imeAdapter, int inputType, int inputFlags, int selectionStart,
            int selectionEnd, EditorInfo outAttrs) {
        ImeUtils.checkOnUiThread();
        if (shouldTriggerDelayedOnCreateInputConnection()) {
            triggerDelayedOnCreateInputConnection(view);
            return null;
        }
        if (DEBUG_LOGS) Log.w(TAG, "initializeAndGet: called from proxy view");
        if (mThreadedInputConnection == null) {
            if (DEBUG_LOGS) Log.w(TAG, "Creating ThreadedInputConnection...");
            mThreadedInputConnection = new ThreadedInputConnection(imeAdapter, mHandler);
        }
        mThreadedInputConnection.initializeOutAttrsOnUiThread(inputType, inputFlags,
                selectionStart, selectionEnd, outAttrs);
        return mThreadedInputConnection;
    }

    private void triggerDelayedOnCreateInputConnection(final View view) {
        if (DEBUG_LOGS) Log.w(TAG, "triggerDelayedOnCreateInputConnection");
        if (mProxyView == null) {
            mProxyView = createProxyView(mHandler, view);
        }
        mProxyView.requestFocus();
        view.getHandler().post(new Runnable() {
            @Override
            public void run() {
                // This is a hack to make InputMethodManager believe that the proxy view
                // now has a focus. As a result, InputMethodManager will think that mProxyView
                // is focused, and will call getHandler() of the view when creating input
                // connection.

                // Step 1: Set mProxyView as InputMethodManager#mNextServedView.
                mProxyView.onWindowFocusChanged(true);

                // Step 2: Have InputMethodManager focus in on mNextServedView.
                // As a result, IMM will call onCreateInputConnection() on mProxyView on the same
                // thread as mProxyView.getHandler(). It will also call subsequent InputConnection
                // methods on this IME thread.
                mInputMethodManagerWrapper.isActive(view);

                // Step 3: Check that the above hack worked.
                // Step 3-1: Wait until activation finishes inside InputMethodManager.
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        // Step 3-2. Run the check on view's handler (mostly UI) thread.
                        // This prevents other views from taking focus in the middle of detection.
                        final Handler viewHandler = view.getHandler();
                        if (viewHandler == null) return;
                        viewHandler.post(new Runnable() {
                            @Override
                            public void run() {
                                assertRegisterProxyViewOnUiThread(view);
                            }
                        });
                    }
                });
            }
        });
    }

    private void assertRegisterProxyViewOnUiThread(View view) {
        // Success.
        if (mInputMethodManagerWrapper.isActive(mProxyView)) {
            mInputMethodUma.recordProxyViewSuccess();
            return;
        }

        // Some other view already took focus. Container view should be active
        // otherwise regardless of whether proxy view is registered or not.
        if (!mInputMethodManagerWrapper.isActive(view)) return;

        if (mThreadedInputConnection == null) {
            // First time and failed. It is highly likely that this does not work systematically.
            mInputMethodUma.recordProxyViewFailure();
            onRegisterProxyViewFailed();
        } else {
            // Most likely that we already lost view focus.
            mInputMethodUma.recordProxyViewDetectionFailure();
        }
    }

    @VisibleForTesting
    protected void onRegisterProxyViewFailed() {
        throw new AssertionError("Failed to register proxy view");
    }

    @Override
    public Handler getHandler() {
        return mHandler;
    }
}