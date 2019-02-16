// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.IntDef;

import com.google.android.play.core.appupdate.AppUpdateInfo;
import com.google.android.play.core.appupdate.testing.FakeAppUpdateManager;
import com.google.android.play.core.install.model.AppUpdateType;
import com.google.android.play.core.tasks.Task;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeActivity;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A wrapper of FakeAppUpdateManager meant to help automatically trigger more update scenarios.  The
 * wrapper isn't meant to be used for a full integration test, but simulating all of the possible
 * error cases is a bit easier to do here.
 */
public class FakeAppUpdateManagerWrapper extends FakeAppUpdateManager {
    private static final int RESULT_IN_APP_UPDATE_FAILED = 1;
    private static final int STEP_DELAY_MS = 5000;

    /** A list of inline update end states that this class can simulate. */
    @IntDef({Type.NO_SIMULATION, Type.NONE, Type.SUCCESS, Type.FAIL_DIALOG_CANCEL,
            Type.FAIL_DIALOG_UPDATE_FAILED, Type.FAIL_DOWNLOAD, Type.FAIL_DOWNLOAD_CANCEL,
            Type.FAIL_INSTALL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        /** No simulation. */
        int NO_SIMULATION = 0;

        /** No update available. */
        int NONE = 1;

        /** The update will be successful. */
        int SUCCESS = 2;

        /** The update will fail because the user will choose cancel when the Play dialog shows. */
        int FAIL_DIALOG_CANCEL = 3;

        /** The update will fail because the dialog will fail with another unknown reason. */
        int FAIL_DIALOG_UPDATE_FAILED = 4;

        /** The update will fail because the download fails. */
        int FAIL_DOWNLOAD = 5;

        /** The update will fail because the download was cancelled.*/
        int FAIL_DOWNLOAD_CANCEL = 6;

        /** The update will fail because it failed to install. */
        int FAIL_INSTALL = 7;
    }

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final @Type int mType;

    /**
     * @param endState at which point should the inline update flow end.
     */
    FakeAppUpdateManagerWrapper(@Type int endState) {
        super(ContextUtils.getApplicationContext());
        mType = endState;

        if (mType != Type.NONE) setUpdateAvailable(10000 /* Figure out a better version? */);
    }

    // FakeAppUpdateManager implementation.
    @Override
    public boolean startUpdateFlowForResult(AppUpdateInfo appUpdateInfo,
            @AppUpdateType int appUpdateType, Activity activity, int requestCode) {
        // TODO(dtrainor): Simulate exceptions being thrown or returning false from the super call.
        boolean success =
                super.startUpdateFlowForResult(appUpdateInfo, appUpdateType, activity, requestCode);
        if (!success) return false;

        assert activity instanceof ChromeActivity : "Unexpected triggering activity.";

        final int resultCode;
        if (mType == Type.FAIL_DIALOG_CANCEL) {
            resultCode = Activity.RESULT_CANCELED;
        } else if (mType == Type.FAIL_DIALOG_UPDATE_FAILED) {
            resultCode = RESULT_IN_APP_UPDATE_FAILED;
        } else {
            resultCode = Activity.RESULT_OK;
        }

        mHandler.postDelayed(() -> {
            triggerDialogResponse((ChromeActivity) activity, requestCode, resultCode);
        }, STEP_DELAY_MS);

        return true;
    }

    @Override
    public Task<Void> completeUpdate() {
        Task<Void> result = super.completeUpdate();

        if (mType == Type.FAIL_INSTALL) {
            mHandler.postDelayed(this::installFails, STEP_DELAY_MS);
        } else {
            mHandler.postDelayed(this::installCompletes, STEP_DELAY_MS);
            // This doesn't actually restart Chrome in this case.
        }

        return result;
    }

    private void triggerDialogResponse(ChromeActivity activity, int requestCode, int resultCode) {
        if (resultCode == Activity.RESULT_OK) {
            userAcceptsUpdate();
        } else if (resultCode == Activity.RESULT_CANCELED) {
            userRejectsUpdate();
        }

        activity.onActivityResult(requestCode, resultCode, null);
        if (resultCode == Activity.RESULT_OK) {
            mHandler.postDelayed(this::triggerDownload, STEP_DELAY_MS);
        }
    }

    private void triggerDownload() {
        downloadStarts();

        if (mType == Type.FAIL_DOWNLOAD) {
            mHandler.postDelayed(this::downloadFails, STEP_DELAY_MS);
        } else if (mType == Type.FAIL_DOWNLOAD_CANCEL) {
            mHandler.postDelayed(this::userCancelsDownload, STEP_DELAY_MS);
        } else {
            mHandler.postDelayed(this::triggerInstall, STEP_DELAY_MS);
        }
    }

    private void triggerInstall() {
        downloadCompletes();
    }
}
