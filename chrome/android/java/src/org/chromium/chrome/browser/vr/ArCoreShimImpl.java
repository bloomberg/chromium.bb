// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;

import com.google.ar.core.ArCoreApk;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.vr.ArCoreShim.InstallStatus;

class ArCoreShimImpl implements ArCoreShim {
    @UsedByReflection("ArCoreJavaUtils.java")
    public ArCoreShimImpl() {}

    @Override
    public InstallStatus requestInstall(Activity activity, boolean userRequestedInstall)
            throws UnavailableDeviceNotCompatibleException,
                   UnavailableUserDeclinedInstallationException {
        try {
            ArCoreApk.InstallStatus installStatus =
                    ArCoreApk.getInstance().requestInstall(activity, userRequestedInstall);
            return mapArCoreApkInstallStatus(installStatus);
        } catch (com.google.ar.core.exceptions.UnavailableDeviceNotCompatibleException e) {
            throw new UnavailableDeviceNotCompatibleException(e);
        } catch (com.google.ar.core.exceptions.UnavailableUserDeclinedInstallationException e) {
            throw new UnavailableUserDeclinedInstallationException(e);
        }
    }

    private InstallStatus mapArCoreApkInstallStatus(ArCoreApk.InstallStatus installStatus) {
        switch (installStatus) {
            case INSTALLED:
                return InstallStatus.INSTALLED;
            case INSTALL_REQUESTED:
                return InstallStatus.INSTALL_REQUESTED;
            default:
                throw new RuntimeException(
                        String.format("Unknown value of InstallStatus: %s", installStatus));
        }
    }
}
