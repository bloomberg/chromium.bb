// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.os.StrictMode;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeTabbedActivity;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;

/**
 * Manages interactions with the VR Shell.
 */
public class VrShellDelegate {
    private static final String TAG = "VrShellDelegate";

    private ChromeTabbedActivity mActivity;

    private boolean mVrShellEnabled;

    private Class<? extends VrShellInterface> mVrShellClass;
    private VrShellInterface mVrShellView;
    private boolean mInVr;
    private int mRestoreSystemUiVisibilityFlag = -1;
    private ViewGroup mParentView;
    private String mVrExtra;

    public VrShellDelegate(ChromeTabbedActivity activity, ViewGroup parentView) {
        mActivity = activity;
        mParentView = parentView;

        mVrShellClass = maybeFindVrShell();
        if (mVrShellClass != null) {
            mVrShellEnabled = true;
            try {
                mVrExtra = (String) mVrShellClass.getField("VR_EXTRA").get(null);
            } catch (IllegalAccessException | IllegalArgumentException | NoSuchFieldException e) {
                Log.e(TAG, "Unable to read VR_EXTRA field", e);
                mVrShellEnabled = false;
            }
        }
    }

    @SuppressWarnings("unchecked")
    private Class<? extends VrShellInterface> maybeFindVrShell() {
        try {
            return (Class<? extends VrShellInterface>) Class
                    .forName("org.chromium.chrome.browser.vr_shell.VrShell");
        } catch (ClassNotFoundException e) {
            return null;
        }
    }

    /**
     * Enters VR Shell, displaying browser UI and tab contents in VR.
     *
     * This function performs native initialization, and so must only be called after native
     * libraries are ready.
     * @Returns Whether or not we are in VR when this function returns.
     */
    public boolean enterVRIfNecessary() {
        if (!mVrShellEnabled) return false;
        if (mInVr) return true;
        // VrShell must be initialized in Landscape mode due to a bug in the GVR library.
        mActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        if (!createVrShell()) {
            mActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
            return false;
        }
        addVrViews();
        setupVrModeWindowFlags();
        mVrShellView.onNativeLibraryReady();
        mVrShellView.setVrModeEnabled(true);
        mInVr = true;
        return true;
    }

    /**
     * Resumes VR Shell.
     */
    public void resumeVR() {
        setupVrModeWindowFlags();
        mVrShellView.resume();
    }

    /**
     * Pauses VR Shell.
     */
    public void pauseVR() {
        mVrShellView.pause();
    }

    /**
     * Exits VR Shell, performing all necessary cleanup.
     * @Returns Whether or not we exited VR.
     */
    public boolean exitVRIfNecessary() {
        if (!mInVr) return false;
        mActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
        mVrShellView.setVrModeEnabled(false);
        mVrShellView.pause();
        removeVrViews();
        clearVrModeWindowFlags();
        destroyVrShell();
        mInVr = false;
        return true;
    }

    private boolean createVrShell() {
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            Constructor<?> vrShellConstructor = mVrShellClass.getConstructor(Activity.class);
            mVrShellView = (VrShellInterface) vrShellConstructor.newInstance(mActivity);
        } catch (InstantiationException | IllegalAccessException | IllegalArgumentException
                | InvocationTargetException | NoSuchMethodException e) {
            Log.e(TAG, "Unable to instantiate VrShell", e);
            return false;
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
        return true;
    }

    private void addVrViews() {
        LayoutParams params = new LayoutParams(LayoutParams.MATCH_PARENT,
                LayoutParams.MATCH_PARENT);
        mParentView.addView(mVrShellView.getContainer(), mParentView.getChildCount(), params);
    }

    private void removeVrViews() {
        ((ViewGroup) mVrShellView.getContainer().getParent())
                .removeView(mVrShellView.getContainer());
    }

    private void setupVrModeWindowFlags() {
        if (mRestoreSystemUiVisibilityFlag == -1) {
            mRestoreSystemUiVisibilityFlag = mActivity.getWindow().getDecorView()
                    .getSystemUiVisibility();
        }
        mActivity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        mActivity.getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }

    private void clearVrModeWindowFlags() {
        mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (mRestoreSystemUiVisibilityFlag != -1) {
            mActivity.getWindow().getDecorView()
                    .setSystemUiVisibility(mRestoreSystemUiVisibilityFlag);
        }
        mRestoreSystemUiVisibilityFlag = -1;
    }

    /**
     * Clean up VrShell, and associated native objects.
     */
    public void destroyVrShell() {
        if (mVrShellView != null) {
            mVrShellView.teardown();
            mVrShellView = null;
        }
    }

    /**
     * Whether or not the intent is a Daydream VR Intent.
     */
    public boolean isVrIntent(Intent intent) {
        if (intent == null) return false;
        return intent.getBooleanExtra(mVrExtra, false);
    }

    /**
     * Whether or not we are currently in VR.
     */
    public boolean isInVR() {
        return mInVr;
    }

    /**
     * @return Whether or not VR Shell is currently enabled.
     */
    public boolean isVrShellEnabled() {
        return mVrShellEnabled;
    }
}
