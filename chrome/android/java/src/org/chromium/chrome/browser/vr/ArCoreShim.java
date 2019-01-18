// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;

/**
 * Interface used to wrap around ArCore SDK Java interface.
 *
 * For detailed documentation of the below methods, please see:
 * https://developers.google.com/ar/reference/java/arcore/reference/com/google/ar/core/ArCoreApk
 */
public interface ArCoreShim {
    /**
     * Equivalent of ARCoreApk.ArInstallStatus enum.
     */
    public enum InstallStatus { INSTALLED, INSTALL_REQUESTED }

    /**
     * Equivalent of ArCoreApk.requestInstall.
     */
    public InstallStatus requestInstall(Activity activity, boolean userRequestedInstall)
            throws UnavailableDeviceNotCompatibleException,
                   UnavailableUserDeclinedInstallationException;

    /**
     * Thrown by requestInstall() when device is not compatible with ARCore.
     */
    public class UnavailableDeviceNotCompatibleException extends Exception {
        public UnavailableDeviceNotCompatibleException(Exception cause) {
            super(cause);
        }
    }

    /**
     * Thrown by requestInstall() when user declined to install ARCore.
     */
    public class UnavailableUserDeclinedInstallationException extends Exception {
        UnavailableUserDeclinedInstallationException(Exception cause) {
            super(cause);
        }
    }
}
