// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.res.Resources;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides a consent dialog shown before entering an immersive VR session.
 */
@JNINamespace("vr")
public class VrConsentDialog implements ModalDialogProperties.Controller {
    @NativeMethods
    /* package */ interface VrConsentUiHelperImpl {
        void onUserConsentResult(long nativeGvrConsentHelperImpl, boolean allowed);
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({UserAction.ALLOWED, UserAction.DENIED, UserAction.CONSENT_FLOW_ABORTED})
    @interface UserAction {
        // Don't reuse or reorder values. If you add something, update NUM_ENTRIES.
        // Also, keep these in sync with ConsentDialogAction enums in
        // XrSessionRequestConsentDialogDelegate
        int ALLOWED = 0;
        int DENIED = 1;
        int CONSENT_FLOW_ABORTED = 2;
        int NUM_ENTRIES = 3;
    }

    private ModalDialogManager mModalDialogManager;
    private VrConsentListener mListener;
    private static long sDialogOpenTimeStampMillis;
    private long mNativeInstance;

    private VrConsentDialog(long instance) {
        mNativeInstance = instance;
    }

    @CalledByNative
    private static VrConsentDialog promptForUserConsent(long instance, final Tab tab) {
        VrConsentDialog dialog = new VrConsentDialog(instance);
        dialog.show(tab.getActivity(), new VrConsentListener() {
            @Override
            public void onUserConsent(boolean allowed) {
                dialog.onUserGesture(allowed);
            }
        });
        return dialog;
    }

    @VisibleForTesting
    protected void onUserGesture(boolean allowed) {
        VrConsentDialogJni.get().onUserConsentResult(mNativeInstance, allowed);
    }

    public void show(@NonNull ChromeActivity activity, @NonNull VrConsentListener listener) {
        mListener = listener;

        Resources resources = activity.getResources();
        PropertyModel model = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                      .with(ModalDialogProperties.CONTROLLER, this)
                                      .with(ModalDialogProperties.TITLE, resources,
                                              R.string.xr_consent_dialog_title)
                                      .with(ModalDialogProperties.MESSAGE, resources,
                                              R.string.xr_consent_dialog_description)
                                      .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                              R.string.xr_consent_dialog_button_allow_and_enter_vr)
                                      .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                              R.string.xr_consent_dialog_button_deny_vr)
                                      .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                                      .build();
        mModalDialogManager = activity.getModalDialogManager();
        mModalDialogManager.showDialog(model, ModalDialogManager.ModalDialogType.TAB);
        sDialogOpenTimeStampMillis = System.currentTimeMillis();
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        } else {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
            mListener.onUserConsent(true);
            logUserAction(UserAction.ALLOWED);
            logConsentFlowDurationWhenConsentGranted();
        } else if (dismissalCause == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
            mListener.onUserConsent(false);
            logUserAction(UserAction.DENIED);
            logConsentFlowDurationWhenConsentNotGranted();
        } else {
            mListener.onUserConsent(false);
            logUserAction(UserAction.CONSENT_FLOW_ABORTED);
            logConsentFlowDurationWhenUserAborted();
        }
    }

    private static void logUserAction(@UserAction int action) {
        // TODO(crbug.com/965538): call c++ functions that update the same histogram instead of
        // calling this java method.
        RecordHistogram.recordEnumeratedHistogram(
                "XR.WebXR.ConsentFlow", action, UserAction.NUM_ENTRIES);
    }

    private static void logConsentFlowDurationWhenConsentGranted() {
        // TODO(crbug.com/965538): call c++ functions that update the same histogram instead of
        // calling this java method.
        RecordHistogram.recordMediumTimesHistogram("XR.WebXR.ConsentFlowDuration.ConsentGranted",
                System.currentTimeMillis() - sDialogOpenTimeStampMillis);
    }

    private static void logConsentFlowDurationWhenConsentNotGranted() {
        // TODO(crbug.com/965538): call c++ functions that update the same histogram instead of
        // calling this java method.
        RecordHistogram.recordMediumTimesHistogram("XR.WebXR.ConsentFlowDuration.ConsentNotGranted",
                System.currentTimeMillis() - sDialogOpenTimeStampMillis);
    }

    private static void logConsentFlowDurationWhenUserAborted() {
        // TODO(crbug.com/965538): call c++ functions that update the same histogram instead of
        // calling this java method.
        RecordHistogram.recordMediumTimesHistogram(
                "XR.WebXR.ConsentFlowDuration.ConsentFlowAborted",
                System.currentTimeMillis() - sDialogOpenTimeStampMillis);
    }

    // TODO(sumankancherla): Add bounce metrics by addressing crbug.com/965538.
}
