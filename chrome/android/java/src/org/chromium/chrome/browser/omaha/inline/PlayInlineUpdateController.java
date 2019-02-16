// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import android.app.Activity;
import android.content.IntentSender.SendIntentException;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.Nullable;

import com.google.android.play.core.appupdate.AppUpdateInfo;
import com.google.android.play.core.appupdate.AppUpdateManager;
import com.google.android.play.core.install.InstallState;
import com.google.android.play.core.install.InstallStateUpdatedListener;
import com.google.android.play.core.install.model.AppUpdateType;
import com.google.android.play.core.install.model.InstallStatus;
import com.google.android.play.core.install.model.UpdateAvailability;

import org.chromium.base.Log;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;

/**
 * Helper class for gluing interactions with the Play store's AppUpdateManager with Chrome.  This
 * involves hooking up to Play as a listener for install state changes, should only happen if we are
 * in the foreground.
 */
public class PlayInlineUpdateController
        implements InlineUpdateController, InstallStateUpdatedListener {
    private static final String TAG = "PlayInline";
    private static final int RESULT_IN_APP_UPDATE_FAILED = 1;
    private static final int REQUEST_CODE = 8123;

    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private final Runnable mCallback;
    private final AppUpdateManager mAppUpdateManager;

    private boolean mEnabled;
    private @Nullable @UpdateState Integer mUpdateState;

    private AppUpdateInfo mAppUpdateInfo;
    private @Nullable @UpdateAvailability Integer mUpdateAvailability;
    private @Nullable @InstallStatus Integer mInstallStatus;

    /**
     * Builds an instance of {@link PlayInlineUpdateController}.
     * @param callback The {@link Runnable} to notify when an inline update state change occurs.
     */
    PlayInlineUpdateController(Runnable callback, AppUpdateManager appUpdateManager) {
        mCallback = callback;
        mAppUpdateManager = appUpdateManager;
        setEnabled(true);
    }

    // InlineUpdateController implementation.
    @Override
    public void setEnabled(boolean enabled) {
        if (mEnabled == enabled) return;
        mEnabled = enabled;

        if (mEnabled) {
            mUpdateState = UpdateState.NONE;
            mAppUpdateManager.registerListener(this);
            pullCurrentState();
        } else {
            mAppUpdateManager.unregisterListener(this);
        }
    }

    @Override
    public @Nullable @UpdateState Integer getStatus() {
        return mUpdateState;
    }

    @Override
    public void startUpdate(Activity activity) {
        try {
            boolean success = mAppUpdateManager.startUpdateFlowForResult(
                    mAppUpdateInfo, AppUpdateType.FLEXIBLE, activity, REQUEST_CODE);
            Log.i(TAG, "startUpdateFlowForResult() returned " + success);
        } catch (SendIntentException exception) {
            mInstallStatus = InstallStatus.FAILED;
            Log.i(TAG, "startUpdateFlowForResult() threw an exception.");
        }
        // TODO(dtrainor): Use success.
    }

    @Override
    public void completeUpdate() {
        mAppUpdateManager.completeUpdate()
                .addOnSuccessListener(unused -> {
                    Log.i(TAG, "completeUpdate() success.");
                    pushStatus();
                })
                .addOnFailureListener(exception -> {
                    Log.i(TAG, "completeUpdate() failed.");
                    mInstallStatus = InstallStatus.FAILED;
                    pushStatus();
                });
    }

    // InstallStateUpdatedListener implementation.
    @Override
    public void onStateUpdate(InstallState state) {
        Log.i(TAG,
                "onStateUpdate(" + state.installStatus() + ", " + state.installErrorCode() + ")");
        mInstallStatus = state.installStatus();
        pushStatus();
    }

    private void pullCurrentState() {
        mAppUpdateManager.getAppUpdateInfo()
                .addOnSuccessListener(info -> {
                    mAppUpdateInfo = info;
                    mUpdateAvailability = info.updateAvailability();
                    mInstallStatus = info.installStatus();
                    Log.i(TAG,
                            "pullCurrentState(" + mUpdateAvailability + ", " + mInstallStatus
                                    + ") success.");
                    pushStatus();
                })
                .addOnFailureListener(exception -> {
                    mAppUpdateInfo = null;
                    mUpdateAvailability = UpdateAvailability.UNKNOWN;
                    mInstallStatus = InstallStatus.UNKNOWN;
                    Log.i(TAG, "pullCurrentState() failed.");
                    pushStatus();
                });
    }

    private void pushStatus() {
        if (!mEnabled || mUpdateAvailability == null || mInstallStatus == null) return;

        @UpdateState
        int newState = toUpdateState(mUpdateAvailability, mInstallStatus);
        if (mUpdateState != null && mUpdateState == newState) return;

        Log.i(TAG, "Pushing inline update state to " + newState);
        mUpdateState = newState;
        mCallback.run();
    }

    private static @UpdateState int toUpdateState(
            @UpdateAvailability int updateAvailability, @InstallStatus int installStatus) {
        @UpdateState
        int newStatus = UpdateState.NONE;

        // Note, use InstallStatus first then UpdateAvailability if InstallStatus doesn't indicate
        // a currently active install.
        switch (installStatus) {
            case InstallStatus.PENDING:
                // Intentional fall through.
            case InstallStatus.DOWNLOADING:
                newStatus = UpdateState.INLINE_UPDATE_DOWNLOADING;
                break;
            case InstallStatus.DOWNLOADED:
                newStatus = UpdateState.INLINE_UPDATE_READY;
                break;
            case InstallStatus.FAILED:
                newStatus = UpdateState.INLINE_UPDATE_FAILED;
                break;
        }

        if (newStatus == UpdateState.NONE) {
            switch (updateAvailability) {
                case UpdateAvailability.UPDATE_AVAILABLE:
                    newStatus = UpdateState.INLINE_UPDATE_AVAILABLE;
                    break;
            }
        }

        return newStatus;
    }
}