// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.autofill.AutofillManager;
import android.view.autofill.AutofillValue;

import org.chromium.base.Log;

import java.lang.ref.WeakReference;

/**
 * The class to call Android's AutofillManager.
 */
@TargetApi(Build.VERSION_CODES.O)
public class AwAutofillManager {
    private static final String TAG = "AwAutofillManager";
    private static final boolean DEBUG = false;

    private static class AutofillInputUIMonitor extends AutofillManager.AutofillCallback {
        private WeakReference<AwAutofillManager> mManager;

        public AutofillInputUIMonitor(AwAutofillManager manager) {
            mManager = new WeakReference<AwAutofillManager>(manager);
        }

        @Override
        public void onAutofillEvent(View view, int virtualId, int event) {
            AwAutofillManager manager = mManager.get();
            if (manager == null) return;
            manager.mIsAutofillInputUIShowing = (event == EVENT_INPUT_SHOWN);
        }
    }

    private AutofillManager mAutofillManager;
    private boolean mIsAutofillInputUIShowing;
    private AutofillInputUIMonitor mMonitor;
    private boolean mDestroyed;
    private boolean mDisabled;

    public AwAutofillManager(Context context) {
        if (DEBUG) Log.i(TAG, "constructor");
        mAutofillManager = context.getSystemService(AutofillManager.class);
        mDisabled = mAutofillManager == null || !mAutofillManager.isEnabled();
        if (mDisabled) return;

        mMonitor = new AutofillInputUIMonitor(this);
        mAutofillManager.registerCallback(mMonitor);
    }

    public void notifyVirtualValueChanged(View parent, int childId, AutofillValue value) {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (DEBUG) Log.i(TAG, "notifyVirtualValueChanged");
        mAutofillManager.notifyValueChanged(parent, childId, value);
    }

    public void commit(int submissionSource) {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (DEBUG) Log.i(TAG, "commit source:" + submissionSource);
        mAutofillManager.commit();
    }

    public void cancel() {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (DEBUG) Log.i(TAG, "cancel");
        mAutofillManager.cancel();
    }

    public void notifyVirtualViewEntered(View parent, int childId, Rect absBounds) {
        // Log warning only when the autofill is triggered.
        if (mDisabled) {
            Log.w(TAG,
                    "WebView autofill is disabled because WebView isn't created with "
                            + "activity context.");
            return;
        }
        if (checkAndWarnIfDestroyed()) return;
        if (DEBUG) Log.i(TAG, "notifyVirtualViewEntered");
        mAutofillManager.notifyViewEntered(parent, childId, absBounds);
    }

    public void notifyVirtualViewExited(View parent, int childId) {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (DEBUG) Log.i(TAG, "notifyVirtualViewExited");
        mAutofillManager.notifyViewExited(parent, childId);
    }

    public void requestAutofill(View parent, int virtualId, Rect absBounds) {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (DEBUG) Log.i(TAG, "requestAutofill");
        mAutofillManager.requestAutofill(parent, virtualId, absBounds);
    }

    public boolean isAutofillInputUIShowing() {
        if (mDisabled || checkAndWarnIfDestroyed()) return false;
        if (DEBUG) Log.i(TAG, "isAutofillInputUIShowing: " + mIsAutofillInputUIShowing);
        return mIsAutofillInputUIShowing;
    }

    public void destroy() {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (DEBUG) Log.i(TAG, "destroy");
        mAutofillManager.unregisterCallback(mMonitor);
        mAutofillManager = null;
        mDestroyed = true;
    }

    private boolean checkAndWarnIfDestroyed() {
        if (mDestroyed) {
            Log.w(TAG, "Application attempted to call on a destroyed AwAutofillManager",
                    new Throwable());
        }
        return mDestroyed;
    }
}
