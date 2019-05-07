// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import android.content.SharedPreferences;
import android.util.Pair;

import com.google.android.play.core.splitinstall.SplitInstallException;
import com.google.android.play.core.splitinstall.SplitInstallManager;
import com.google.android.play.core.splitinstall.SplitInstallManagerFactory;
import com.google.android.play.core.splitinstall.SplitInstallRequest;
import com.google.android.play.core.splitinstall.SplitInstallSessionState;
import com.google.android.play.core.splitinstall.SplitInstallStateUpdatedListener;
import com.google.android.play.core.splitinstall.model.SplitInstallErrorCode;
import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.module_installer.ModuleInstallerBackend.OnFinishedListener;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Backend that uses the Play Core SDK to download a module from Play and install it subsequently.
 */
/* package */ class PlayCoreModuleInstallerBackend
        extends ModuleInstallerBackend implements SplitInstallStateUpdatedListener {
    private static final String TAG = "PlayCoreModInBackend";
    private static final String KEY_MODULES_REQUESTED_PREVIOUSLY =
            "key_modules_requested_previously";
    private final Map<String, Pair<Long, Boolean>> mInstallStartTimeMap = new HashMap<>();
    private final SplitInstallManager mManager;
    private boolean mIsClosed;

    // FeatureModuleInstallStatus defined in //tools/metrics/histograms/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    private static final int INSTALL_STATUS_SUCCESS = 0;
    // private static final int INSTALL_STATUS_FAILURE = 1; [DEPRECATED]
    // private static final int INSTALL_STATUS_REQUEST_ERROR = 2; [DEPRECATED]
    private static final int INSTALL_STATUS_CANCELLATION = 3;
    private static final int INSTALL_STATUS_ACCESS_DENIED = 4;
    private static final int INSTALL_STATUS_ACTIVE_SESSIONS_LIMIT_EXCEEDED = 5;
    private static final int INSTALL_STATUS_API_NOT_AVAILABLE = 6;
    private static final int INSTALL_STATUS_INCOMPATIBLE_WITH_EXISTING_SESSION = 7;
    private static final int INSTALL_STATUS_INSUFFICIENT_STORAGE = 8;
    private static final int INSTALL_STATUS_INVALID_REQUEST = 9;
    private static final int INSTALL_STATUS_MODULE_UNAVAILABLE = 10;
    private static final int INSTALL_STATUS_NETWORK_ERROR = 11;
    private static final int INSTALL_STATUS_NO_ERROR = 12;
    private static final int INSTALL_STATUS_SERVICE_DIED = 13;
    private static final int INSTALL_STATUS_SESSION_NOT_FOUND = 14;
    private static final int INSTALL_STATUS_SPLITCOMPAT_COPY_ERROR = 15;
    private static final int INSTALL_STATUS_SPLITCOMPAT_EMULATION_ERROR = 16;
    private static final int INSTALL_STATUS_SPLITCOMPAT_VERIFICATION_ERROR = 17;
    private static final int INSTALL_STATUS_INTERNAL_ERROR = 18;
    private static final int INSTALL_STATUS_UNKNOWN_SPLITINSTALL_ERROR = 19;
    private static final int INSTALL_STATUS_UNKNOWN_REQUEST_ERROR = 20;

    // Keep this one at the end and increment appropriately when adding new status.
    private static final int INSTALL_STATUS_COUNT = 21;

    public PlayCoreModuleInstallerBackend(OnFinishedListener listener) {
        super(listener);
        mManager = SplitInstallManagerFactory.create(ContextUtils.getApplicationContext());
        mManager.registerListener(this);
    }

    @Override
    public void install(String moduleName) {
        assert !mIsClosed;

        // Record start time in order to later report the install duration via UMA. We want to make
        // a difference between modules that have been requested first before and after the last
        // Chrome start. Modules that have been requested before may install quicker as they may be
        // installed form cache. To do this, we use shared prefs to track modules previously
        // requested.
        assert !mInstallStartTimeMap.containsKey(moduleName);
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> modulesRequestedPreviously =
                prefs.getStringSet(KEY_MODULES_REQUESTED_PREVIOUSLY, new HashSet<String>());
        mInstallStartTimeMap.put(moduleName,
                Pair.create(System.currentTimeMillis(),
                        modulesRequestedPreviously.contains(moduleName)));
        Set<String> newModulesRequestedPreviously = new HashSet<>(modulesRequestedPreviously);
        newModulesRequestedPreviously.add(moduleName);
        SharedPreferences.Editor editor = prefs.edit();
        editor.putStringSet(KEY_MODULES_REQUESTED_PREVIOUSLY, newModulesRequestedPreviously);
        editor.apply();

        SplitInstallRequest request =
                SplitInstallRequest.newBuilder().addModule(moduleName).build();

        mManager.startInstall(request).addOnFailureListener(exception -> {
            int status = exception instanceof SplitInstallException
                    ? getHistogramCode(((SplitInstallException) exception).getErrorCode())
                    : INSTALL_STATUS_UNKNOWN_REQUEST_ERROR;
            Log.e(TAG, "Failed to request module '%s': error code %s", moduleName, status);
            // If we reach this error condition |onStateUpdate| won't be called. Thus, call
            // |onFinished| here.
            finish(false, Collections.singletonList(moduleName), status);
        });
    }

    @Override
    public void installDeferred(String moduleName) {
        assert !mIsClosed;
        mManager.deferredInstall(Collections.singletonList(moduleName));
    }

    @Override
    public void close() {
        assert !mIsClosed;
        mManager.unregisterListener(this);
        mIsClosed = true;
    }

    @Override
    public void onStateUpdate(SplitInstallSessionState state) {
        assert !mIsClosed;
        switch (state.status()) {
            case SplitInstallSessionStatus.INSTALLED:
                finish(true, state.moduleNames(), INSTALL_STATUS_SUCCESS);
                break;
            case SplitInstallSessionStatus.CANCELED:
            case SplitInstallSessionStatus.FAILED:
                int status = state.status() == SplitInstallSessionStatus.CANCELED
                        ? INSTALL_STATUS_CANCELLATION
                        : getHistogramCode(state.errorCode());
                Log.e(TAG, "Failed to install modules '%s': error code %s", state.moduleNames(),
                        status);
                finish(false, state.moduleNames(), status);
                break;
        }
    }

    private void finish(boolean success, List<String> moduleNames, int eventId) {
        for (String name : moduleNames) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.FeatureModules.InstallStatus." + name, eventId, INSTALL_STATUS_COUNT);

            assert mInstallStartTimeMap.containsKey(name);
            if (success) {
                Pair<Long, Boolean> moduleInfo = mInstallStartTimeMap.get(name);
                long installDurationMs = System.currentTimeMillis() - moduleInfo.first;
                String histogramSubname =
                        moduleInfo.second ? "CachedInstallDuration" : "UncachedInstallDuration";
                RecordHistogram.recordLongTimesHistogram(
                        "Android.FeatureModules." + histogramSubname + "." + name,
                        installDurationMs);
            }
        }
        onFinished(success, moduleNames);
    }

    /**
     * Gets the UMA code based on a SplitInstall error code
     * @param errorCode The error code
     * @return int The User Metric Analysis code
     */
    private int getHistogramCode(@SplitInstallErrorCode int errorCode) {
        switch (errorCode) {
            case SplitInstallErrorCode.ACCESS_DENIED:
                return INSTALL_STATUS_ACCESS_DENIED;
            case SplitInstallErrorCode.ACTIVE_SESSIONS_LIMIT_EXCEEDED:
                return INSTALL_STATUS_ACTIVE_SESSIONS_LIMIT_EXCEEDED;
            case SplitInstallErrorCode.API_NOT_AVAILABLE:
                return INSTALL_STATUS_API_NOT_AVAILABLE;
            case SplitInstallErrorCode.INCOMPATIBLE_WITH_EXISTING_SESSION:
                return INSTALL_STATUS_INCOMPATIBLE_WITH_EXISTING_SESSION;
            case SplitInstallErrorCode.INSUFFICIENT_STORAGE:
                return INSTALL_STATUS_INSUFFICIENT_STORAGE;
            case SplitInstallErrorCode.INVALID_REQUEST:
                return INSTALL_STATUS_INVALID_REQUEST;
            case SplitInstallErrorCode.MODULE_UNAVAILABLE:
                return INSTALL_STATUS_MODULE_UNAVAILABLE;
            case SplitInstallErrorCode.NETWORK_ERROR:
                return INSTALL_STATUS_NETWORK_ERROR;
            case SplitInstallErrorCode.NO_ERROR:
                return INSTALL_STATUS_NO_ERROR;
            case SplitInstallErrorCode.SERVICE_DIED:
                return INSTALL_STATUS_SERVICE_DIED;
            case SplitInstallErrorCode.SESSION_NOT_FOUND:
                return INSTALL_STATUS_SESSION_NOT_FOUND;
            case SplitInstallErrorCode.SPLITCOMPAT_COPY_ERROR:
                return INSTALL_STATUS_SPLITCOMPAT_COPY_ERROR;
            case SplitInstallErrorCode.SPLITCOMPAT_EMULATION_ERROR:
                return INSTALL_STATUS_SPLITCOMPAT_EMULATION_ERROR;
            case SplitInstallErrorCode.SPLITCOMPAT_VERIFICATION_ERROR:
                return INSTALL_STATUS_SPLITCOMPAT_VERIFICATION_ERROR;
            case SplitInstallErrorCode.INTERNAL_ERROR:
                return INSTALL_STATUS_INTERNAL_ERROR;
            default:
                return INSTALL_STATUS_UNKNOWN_SPLITINSTALL_ERROR;
        }
    }
}
