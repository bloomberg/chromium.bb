// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.autofill.AutofillManager;
import android.view.autofill.AutofillValue;

import org.chromium.base.Log;

import java.lang.ref.WeakReference;

/**
 * The class to call Android's AutofillManager.
 */
// TODO(michaelbai): Extend this class to provide instrumentation test. http://crbug.com/717658.
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

    public AwAutofillManager(Context context) {
        mAutofillManager = context.getSystemService(AutofillManager.class);
        mMonitor = new AutofillInputUIMonitor(this);
        mAutofillManager.registerCallback(mMonitor);
    }

    public void notifyVirtualValueChanged(View parent, int childId, AutofillValue value) {
        if (isDestroyed()) return;
        mAutofillManager.notifyValueChanged(parent, childId, value);
    }

    public void commit() {
        if (isDestroyed()) return;
        mAutofillManager.commit();
    }

    public void cancel() {
        if (isDestroyed()) return;
        mAutofillManager.cancel();
    }

    public void notifyVirtualViewEntered(View parent, int childId, Rect absBounds) {
        if (isDestroyed()) return;
        mAutofillManager.notifyViewEntered(parent, childId, absBounds);
    }

    public void notifyVirtualViewExited(View parent, int childId) {
        if (isDestroyed()) return;
        mAutofillManager.notifyViewExited(parent, childId);
    }

    public void requestAutofill(View parent, int virtualId, Rect absBounds) {
        if (isDestroyed()) return;
        mAutofillManager.requestAutofill(parent, virtualId, absBounds);
    }

    public boolean isAutofillInputUIShowing() {
        if (isDestroyed()) return false;
        return mIsAutofillInputUIShowing;
    }

    public void destroy() {
        if (isDestroyed()) return;
        mAutofillManager.unregisterCallback(mMonitor);
        mAutofillManager = null;
    }

    private boolean isDestroyed() {
        if (mAutofillManager == null) {
            Log.w(TAG, "Application attempted to call on a destroyed AwAutofillManager",
                    new Throwable());
        }
        return mAutofillManager == null;
    }
}
