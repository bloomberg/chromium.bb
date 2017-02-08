// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.content.Context;
import android.os.StrictMode;

import com.google.vr.ndk.base.AndroidCompat;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;

/**
 * Builder class to create all VR related classes. These VR classes are behind the same build time
 * flag as this class. So no reflection is necessary when create them.
 */
@UsedByReflection("VrShellDelegate.java")
public class VrClassesWrapperImpl implements VrClassesWrapper {
    private static final String TAG = "VrClassesWrapperImpl";
    private final Context mContext;

    @UsedByReflection("VrShellDelegate.java")
    public VrClassesWrapperImpl(ChromeActivity activity) {
        mContext = activity;
    }

    @UsedByReflection("ChromeInstrumentationTestRunner.java")
    @VisibleForTesting
    public VrClassesWrapperImpl(Context context) {
        mContext = context;
    }

    @Override
    public NonPresentingGvrContext createNonPresentingGvrContext() {
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            return new NonPresentingGvrContextImpl((ChromeActivity) mContext);
        } catch (Exception ex) {
            Log.e(TAG, "Unable to instantiate NonPresentingGvrContextImpl", ex);
            return null;
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @Override
    public VrShell createVrShell(VrShellDelegate delegate,
            CompositorViewHolder compositorViewHolder) {
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            return new VrShellImpl((ChromeActivity) mContext, delegate, compositorViewHolder);
        } catch (Exception ex) {
            Log.e(TAG, "Unable to instantiate VrShellImpl", ex);
            return null;
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @Override
    public VrDaydreamApi createVrDaydreamApi() {
        return new VrDaydreamApiImpl(mContext);
    }

    @Override
    public VrCoreVersionChecker createVrCoreVersionChecker() {
        return new VrCoreVersionCheckerImpl();
    }

    @Override
    public void setVrModeEnabled(boolean enabled) {
        AndroidCompat.setVrModeEnabled((ChromeActivity) mContext, enabled);
    }
}
