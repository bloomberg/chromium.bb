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

    public AwAutofillManager(Context context) {
        if (AwContents.activityFromContext(context) == null) {
            Log.w(TAG,
                    "WebView autofill is disabled because WebView isn't created with "
                            + "activity context.");
            return;
        }
        mAutofillManager = context.getSystemService(AutofillManager.class);
        mMonitor = new AutofillInputUIMonitor(this);
        mAutofillManager.registerCallback(mMonitor);
    }

    public void notifyVirtualValueChanged(View parent, int childId, AutofillValue value) {
        if (isDestroyed() || mAutofillManager == null) return;
        mAutofillManager.notifyValueChanged(parent, childId, value);
    }

    public void commit() {
        if (isDestroyed() || mAutofillManager == null) return;
        mAutofillManager.commit();
    }

    public void cancel() {
        if (isDestroyed() || mAutofillManager == null) return;
        mAutofillManager.cancel();
    }

    public void notifyVirtualViewEntered(View parent, int childId, Rect absBounds) {
        if (isDestroyed() || mAutofillManager == null) return;
        mAutofillManager.notifyViewEntered(parent, childId, absBounds);
    }

    public void notifyVirtualViewExited(View parent, int childId) {
        if (isDestroyed() || mAutofillManager == null) return;
        mAutofillManager.notifyViewExited(parent, childId);
    }

    public void requestAutofill(View parent, int virtualId, Rect absBounds) {
        if (isDestroyed() || mAutofillManager == null) return;
        mAutofillManager.requestAutofill(parent, virtualId, absBounds);
    }

    public boolean isAutofillInputUIShowing() {
        if (isDestroyed() || mAutofillManager == null) return false;
        return mIsAutofillInputUIShowing;
    }

    public void destroy() {
        if (isDestroyed() || mAutofillManager == null) return;
        mAutofillManager.unregisterCallback(mMonitor);
        mAutofillManager = null;
        mDestroyed = true;
    }

    private boolean isDestroyed() {
        if (mDestroyed) {
            Log.w(TAG, "Application attempted to call on a destroyed AwAutofillManager",
                    new Throwable());
        }
        return mDestroyed;
    }
}
