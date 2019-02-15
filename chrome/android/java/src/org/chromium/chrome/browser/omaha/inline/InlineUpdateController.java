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

import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;

/**
 * Helper class for gluing interactions with the Play store's AppUpdateManager with Chrome.  This
 * involves hooking up to Play as a listener for install state changes, should only happen if we are
 * in the foreground.
 */
public class InlineUpdateController implements InstallStateUpdatedListener {
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
     * Builds an instance of {@link InlineUpdateController}.
     * @param callback The {@link Runnable} to notify when an inline update state change occurs.
     */
    public InlineUpdateController(Runnable callback) {
        mCallback = callback;
        mAppUpdateManager = InlineAppUpdateManagerFactory.create();

        setEnabled(true);
    }

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

    /**
     * @return The current state of the inline update process.  May be {@code null} if the state
     * hasn't been determined yet.
     */
    public @Nullable @UpdateState Integer getStatus() {
        return mUpdateState;
    }

    /**
     * Starts the update, if possible.  This will send an {@link Intent} out to play, which may
     * cause Chrome to move to the background.
     * @param activity The {@link Activity} to use to interact with Play.
     */
    public void startUpdate(Activity activity) {
        try {
            boolean success = mAppUpdateManager.startUpdateFlowForResult(
                    mAppUpdateInfo, AppUpdateType.FLEXIBLE, activity, REQUEST_CODE);
        } catch (SendIntentException exception) {
            mInstallStatus = InstallStatus.FAILED;
        }
        // TODO(dtrainor): Use success.
    }

    /**
     * Completes the Play installation process, if possible.  This may cause Chrome to restart.
     */
    public void completeUpdate() {
        mAppUpdateManager.completeUpdate()
                .addOnSuccessListener(unused -> { pushStatus(); })
                .addOnFailureListener(exception -> {
                    mInstallStatus = InstallStatus.FAILED;
                    pushStatus();
                });
    }

    // InstallStateUpdatedListener implementation.
    @Override
    public void onStateUpdate(InstallState state) {
        mInstallStatus = state.installStatus();
        pushStatus();
    }

    private void pullCurrentState() {
        mAppUpdateManager.getAppUpdateInfo()
                .addOnSuccessListener(info -> {
                    mAppUpdateInfo = info;
                    mUpdateAvailability = info.updateAvailability();
                    mInstallStatus = info.installStatus();
                    pushStatus();
                })
                .addOnFailureListener(exception -> {
                    mAppUpdateInfo = null;
                    mUpdateAvailability = UpdateAvailability.UNKNOWN;
                    mInstallStatus = InstallStatus.UNKNOWN;
                    pushStatus();
                });
    }

    private void pushStatus() {
        if (!mEnabled || mUpdateAvailability == null || mInstallStatus == null) return;

        @UpdateState
        int newState = toUpdateState(mUpdateAvailability, mInstallStatus);
        if (mUpdateState != null && mUpdateState == newState) return;

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