// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import com.google.android.play.core.splitinstall.SplitInstallManager;
import com.google.android.play.core.splitinstall.SplitInstallManagerFactory;
import com.google.android.play.core.splitinstall.SplitInstallRequest;
import com.google.android.play.core.splitinstall.SplitInstallSessionState;
import com.google.android.play.core.splitinstall.SplitInstallStateUpdatedListener;
import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.module_installer.ModuleInstallerBackend.OnFinishedListener;

import java.util.Collections;
import java.util.List;

/**
 * Backend that uses the Play Core SDK to download a module from Play and install it subsequently.
 */
/* package */ class PlayCoreModuleInstallerBackend
        extends ModuleInstallerBackend implements SplitInstallStateUpdatedListener {
    private static final String TAG = "PlayCoreModInBackend";
    private final SplitInstallManager mManager;
    private boolean mIsClosed;

    // FeatureModuleInstallStatus defined in //tools/metrics/histograms/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    private static final int INSTALL_STATUS_SUCCESS = 0;
    private static final int INSTALL_STATUS_FAILURE = 1;
    private static final int INSTALL_STATUS_REQUEST_ERROR = 2;
    private static final int INSTALL_STATUS_CANCELLATION = 3;
    // Keep this one at the end and increment appropriately when adding new status.
    private static final int INSTALL_STATUS_COUNT = 4;

    public PlayCoreModuleInstallerBackend(OnFinishedListener listener) {
        super(listener);
        mManager = SplitInstallManagerFactory.create(ContextUtils.getApplicationContext());
        mManager.registerListener(this);
    }

    @Override
    public void install(String moduleName) {
        assert !mIsClosed;
        SplitInstallRequest request =
                SplitInstallRequest.newBuilder().addModule(moduleName).build();
        mManager.startInstall(request).addOnFailureListener(errorCode -> {
            Log.e(TAG, "Failed to request module '%s': error code %s", moduleName, errorCode);
            // If we reach this error condition |onStateUpdate| won't be called. Thus, call
            // |onFinished| here.
            finish(false, Collections.singletonList(moduleName), INSTALL_STATUS_REQUEST_ERROR);
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
                Log.e(TAG, "Failed to install modules '%s': error code %s", state.moduleNames(),
                        state.status());
                int status = state.status() == SplitInstallSessionStatus.CANCELED
                        ? INSTALL_STATUS_CANCELLATION
                        : INSTALL_STATUS_FAILURE;
                finish(false, state.moduleNames(), status);
                break;
        }
    }

    private void finish(boolean success, List<String> moduleNames, int eventId) {
        for (String name : moduleNames) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.FeatureModules.InstallStatus." + name, eventId, INSTALL_STATUS_COUNT);
        }
        onFinished(success, moduleNames);
    }
}
