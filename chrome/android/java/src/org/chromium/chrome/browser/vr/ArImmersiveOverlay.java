// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.pm.ActivityInfo;
import android.support.annotation.NonNull;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.content_public.browser.ScreenOrientationDelegate;
import org.chromium.content_public.browser.ScreenOrientationProvider;

/**
 * Provides a fullscreen overlay for immersive AR mode.
 */
public class ArImmersiveOverlay
        implements SurfaceHolder.Callback2, View.OnTouchListener, ScreenOrientationDelegate {
    private static final String TAG = "ArImmersiveOverlay";
    private static final boolean DEBUG_LOGS = false;

    private ArCoreJavaUtils mArCoreJavaUtils;
    private ChromeActivity mActivity;
    private boolean mSurfaceReportedReady;
    private Integer mRestoreOrientation;
    private boolean mCleanupInProgress;
    private SurfaceUiWrapper mSurfaceUi;

    public void show(@NonNull ChromeActivity activity, @NonNull ArCoreJavaUtils caller) {
        if (DEBUG_LOGS) Log.i(TAG, "constructor");
        mArCoreJavaUtils = caller;
        mActivity = activity;

        // Choose a concrete implementation to create a drawable Surface and make it fullscreen.
        // It forwards SurfaceHolder callbacks and touch events to this ArImmersiveOverlay object.
        mSurfaceUi = new SurfaceUiDialog(this);
    }

    private interface SurfaceUiWrapper {
        public void onSurfaceVisible();
        public void destroy();
    }

    private class SurfaceUiDialog implements SurfaceUiWrapper, DialogInterface.OnCancelListener {
        private Dialog mDialog;
        // Android supports multiple variants of fullscreen applications. Currently, we use a
        // fullscreen layout with translucent navigation bar, where the content shows behind the
        // navigation bar. Alternatively, we could add FLAG_HIDE_NAVIGATION and
        // FLAG_IMMERSIVE_STICKY to hide the navigation bar, but then we'd need to show a "pull from
        // top and press back button to exit" prompt.
        private static final int VISIBILITY_FLAGS_IMMERSIVE = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN;

        public SurfaceUiDialog(ArImmersiveOverlay parent) {
            // Create a fullscreen dialog and set the system / navigation bars translucent.
            mDialog = new Dialog(mActivity, android.R.style.Theme_NoTitleBar_Fullscreen);
            mDialog.getWindow().setBackgroundDrawable(null);
            int wmFlags = WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS
                    | WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION;
            mDialog.getWindow().addFlags(wmFlags);
            mDialog.getWindow().takeSurface(parent);
            View view = mDialog.getWindow().getDecorView();
            view.setSystemUiVisibility(VISIBILITY_FLAGS_IMMERSIVE);
            view.setOnTouchListener(parent);
            mDialog.setOnCancelListener(this);
            mDialog.getWindow().setLayout(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            mDialog.show();
        }

        @Override // SurfaceUiWrapper
        public void onSurfaceVisible() {}

        @Override // SurfaceUiWrapper
        public void destroy() {
            mDialog.dismiss();
        }

        @Override // DialogInterface.OnCancelListener
        public void onCancel(DialogInterface dialog) {
            if (DEBUG_LOGS) Log.i(TAG, "onCancel");
            cleanupAndExit();
        }
    }

    @Override // View.OnTouchListener
    public boolean onTouch(View v, MotionEvent ev) {
        // Only forward primary actions, ignore more complex events such as secondary pointer
        // touches. Ignore batching since we're only sending one ray pose per frame.
        if (ev.getAction() == MotionEvent.ACTION_DOWN || ev.getAction() == MotionEvent.ACTION_MOVE
                || ev.getAction() == MotionEvent.ACTION_UP) {
            boolean touching = ev.getAction() != MotionEvent.ACTION_UP;
            if (DEBUG_LOGS) Log.i(TAG, "onTouch touching=" + touching);
            mArCoreJavaUtils.onDrawingSurfaceTouch(touching, ev.getX(0), ev.getY(0));
        }
        return true;
    }

    @Override // ScreenOrientationDelegate
    public boolean canUnlockOrientation(Activity activity, int defaultOrientation) {
        if (mActivity == activity && mRestoreOrientation != null) {
            mRestoreOrientation = defaultOrientation;
            return false;
        }
        return true;
    }

    @Override // ScreenOrientationDelegate
    public boolean canLockOrientation() {
        return false;
    }

    @Override // SurfaceHolder.Callback2
    public void surfaceCreated(SurfaceHolder holder) {
        if (DEBUG_LOGS) Log.i(TAG, "surfaceCreated");
        // Do nothing here, we'll handle setup on the following surfaceChanged.
    }

    @Override // SurfaceHolder.Callback2
    public void surfaceRedrawNeeded(SurfaceHolder holder) {
        if (DEBUG_LOGS) Log.i(TAG, "surfaceRedrawNeeded");
    }

    @Override // SurfaceHolder.Callback2
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // WebXR immersive sessions don't support resize, so use the first reported size.
        // We shouldn't get resize events since we're using FLAG_LAYOUT_STABLE and are
        // locking screen orientation.
        if (mSurfaceReportedReady) {
            int rotation = mActivity.getWindowManager().getDefaultDisplay().getRotation();
            if (DEBUG_LOGS)
                Log.i(TAG,
                        "surfaceChanged ignoring change to width=" + width + " height=" + height
                                + " rotation=" + rotation);
            return;
        }

        // Save current orientation mode, and then lock current orientation.
        ScreenOrientationProvider.getInstance().setOrientationDelegate(this);
        if (mRestoreOrientation == null) {
            mRestoreOrientation = mActivity.getRequestedOrientation();
        }
        mActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LOCKED);

        int rotation = mActivity.getWindowManager().getDefaultDisplay().getRotation();
        if (DEBUG_LOGS)
            Log.i(TAG, "surfaceChanged size=" + width + "x" + height + " rotation=" + rotation);
        mArCoreJavaUtils.onDrawingSurfaceReady(holder.getSurface(), rotation, width, height);
        mSurfaceReportedReady = true;

        // Show a toast with instructions how to exit fullscreen mode now if necessary.
        // Not currently needed for Dialog mode since that still has a visible navigation
        // bar with Back button.
        mSurfaceUi.onSurfaceVisible();
    }

    @Override // SurfaceHolder.Callback2
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (DEBUG_LOGS) Log.i(TAG, "surfaceDestroyed");
        cleanupAndExit();
    }

    public void cleanupAndExit() {
        if (DEBUG_LOGS) Log.i(TAG, "cleanupAndExit");

        // Avoid duplicate cleanup if we're exiting via ArCoreJavaUtils's endSession.
        // That triggers cleanupAndExit -> remove SurfaceView -> surfaceDestroyed -> cleanupAndExit.
        if (mCleanupInProgress) return;
        mCleanupInProgress = true;

        mSurfaceUi.destroy();

        // Restore orientation.
        ScreenOrientationProvider.getInstance().setOrientationDelegate(null);
        if (mRestoreOrientation != null) mActivity.setRequestedOrientation(mRestoreOrientation);
        mRestoreOrientation = null;

        // The surface is destroyed when exiting via "back" button, but also in other lifecycle
        // situations such as switching apps or toggling the phone's power button. Treat each of
        // these as exiting the immersive session. We need to run the destroy callbacks to ensure
        // consistent state after non-exiting lifecycle events.
        mArCoreJavaUtils.onDrawingSurfaceDestroyed();
    }
}
