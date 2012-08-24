// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Build;
import android.util.Log;
import android.view.Choreographer;
import android.view.View;
import android.view.WindowManager;

/**
 * Notifies clients of the default displays's vertical sync pulses.
 */
public class VSyncMonitor {
    public interface Listener {
        /**
         * Called very soon after the start of the display's vertical sync period.
         * @param monitor The VSyncMonitor that triggered the signal.
         * @param vsyncTimeMicros Absolute frame time in microseconds.
         */
        public void onVSync(VSyncMonitor monitor, long vsyncTimeMicros);
    }

    /** Time source used for test the timeout functionality. */
    interface TestTimeSource {
        public long currentTimeMillis();
    }

    // To save battery, we don't run vsync more than this time unless requestUpdate() is called.
    public static final long VSYNC_TIMEOUT_MILLISECONDS = 3000;

    private static final long MICROSECONDS_PER_SECOND = 1000000;
    private static final long NANOSECONDS_PER_MICROSECOND = 1000;

    private static final String TAG = VSyncMonitor.class.getName();

    // Display refresh rate as reported by the system.
    private long mRefreshPeriodMicros;

    // Last time requestUpdate() was called.
    private long mLastUpdateRequestMillis;

    // Whether we are currently getting notified of vsync events.
    private boolean mVSyncCallbackActive = false;

    // Choreographer is used to detect vsync on >= JB.
    private Choreographer mChoreographer;
    private Choreographer.FrameCallback mFrameCallback;

    private Listener mListener;
    private View mView;
    private VSyncTerminator mVSyncTerminator;
    private TestTimeSource mTestTimeSource;

    // VSyncTerminator keeps vsync running for a period of VSYNC_TIMEOUT_MILLISECONDS. If there is
    // no update request during this period, the monitor will be stopped automatically.
    private class VSyncTerminator implements Runnable {
        public void run() {
            if (currentTimeMillis() > mLastUpdateRequestMillis +
                VSYNC_TIMEOUT_MILLISECONDS) {
                stop();
            } else if (mVSyncTerminator == this) {
                mView.postDelayed(this, VSYNC_TIMEOUT_MILLISECONDS);
            }
        }
    }

    /**
     * Constructor.
     *
     * @param context Resource context.
     * @param view View to be used for timeout scheduling.
     * @param listener Listener to be notified of vsync events.
     */
    public VSyncMonitor(Context context, View view, Listener listener) {
        mListener = listener;
        mView = view;

        float refreshRate = ((WindowManager) context.getSystemService(Context.WINDOW_SERVICE))
                .getDefaultDisplay().getRefreshRate();
        if (refreshRate <= 0) {
            refreshRate = 60;
        }
        mRefreshPeriodMicros = (long) (MICROSECONDS_PER_SECOND / refreshRate);

        // Use Choreographer on JB+ to get notified of vsync.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            mChoreographer = Choreographer.getInstance();
            mFrameCallback = new Choreographer.FrameCallback() {
                @Override
                public void doFrame(long frameTimeNanos) {
                    postVSyncCallback();
                    mListener.onVSync(VSyncMonitor.this,
                                      frameTimeNanos / NANOSECONDS_PER_MICROSECOND);
                }
            };
        }
    }

    /**
     * Returns the time interval between two consecutive vsync pulses in microseconds.
     */
    public long getVSyncPeriodInMicroseconds() {
        return mRefreshPeriodMicros;
    }

    /**
     * Determine whether a true vsync signal is available on this platform.
     */
    public boolean isVSyncSignalAvailable() {
        return mChoreographer != null;
    }

    /**
     * Request to be notified of display vsync events. Listener.onVSync() will be called soon after
     * the upcoming vsync pulses. If VSYNC_TIMEOUT_MILLISECONDS passes after the last time this
     * function is called, the updates will cease automatically.
     *
     * This function throws an IllegalStateException if isVSyncSignalAvailable() returns false.
     */
    public void requestUpdate() {
        if (!isVSyncSignalAvailable()) {
            throw new IllegalStateException("VSync signal not available");
        }
        mLastUpdateRequestMillis = currentTimeMillis();
        if (!mVSyncCallbackActive) {
            mVSyncCallbackActive = true;
            mVSyncTerminator = new VSyncTerminator();
            mView.postDelayed(mVSyncTerminator, VSYNC_TIMEOUT_MILLISECONDS);
            postVSyncCallback();
        }
    }

    private void postVSyncCallback() {
        if (!mVSyncCallbackActive) {
            return;
        }
        mChoreographer.postFrameCallback(mFrameCallback);
    }

    /**
     * Stop reporting vsync events. Note that at most one pending vsync event can still be delivered
     * after this function is called.
     */
    public void stop() {
        mVSyncCallbackActive = false;
        mVSyncTerminator = null;
    }

    public boolean isActive() {
        return mVSyncCallbackActive;
    }

    void setTestDependencies(View testView, TestTimeSource testTimeSource) {
        mView = testView;
        mTestTimeSource = testTimeSource;
    }

    private long currentTimeMillis() {
        if (mTestTimeSource != null) {
            return mTestTimeSource.currentTimeMillis();
        }
        return System.currentTimeMillis();
    }
}
