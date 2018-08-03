// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.os.Build;

import com.google.vr.ndk.base.Version;
import com.google.vr.vrcore.base.api.VrCoreNotAvailableException;
import com.google.vr.vrcore.base.api.VrCoreUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.chrome.browser.vr.VrCoreInfo.GvrVersion;

/**
 * Helper class to check if VrCore version is compatible with Chromium.
 */
public class VrCoreVersionChecker {
    private static final String TAG = "VrCoreVersionChecker";

    public static final String VR_CORE_PACKAGE_ID = "com.google.vr.vrcore";

    private static final int MIN_SDK_VERSION = Build.VERSION_CODES.KITKAT;

    public VrCoreInfo getVrCoreInfo() {
        if (Build.VERSION.SDK_INT < MIN_SDK_VERSION) {
            return new VrCoreInfo(null, VrCoreCompatibility.VR_NOT_SUPPORTED);
        }
        try {
            String vrCoreSdkLibraryVersionString =
                    VrCoreUtils.getVrCoreSdkLibraryVersion(ContextUtils.getApplicationContext());
            Version vrCoreSdkLibraryVersion = Version.parse(vrCoreSdkLibraryVersionString);
            Version targetSdkLibraryVersion =
                    Version.parse(com.google.vr.ndk.base.BuildConstants.VERSION);
            GvrVersion gvrVersion = new GvrVersion(vrCoreSdkLibraryVersion.majorVersion,
                    vrCoreSdkLibraryVersion.minorVersion, vrCoreSdkLibraryVersion.patchVersion);
            if (!vrCoreSdkLibraryVersion.isAtLeast(targetSdkLibraryVersion)) {
                return new VrCoreInfo(gvrVersion, VrCoreCompatibility.VR_OUT_OF_DATE);
            }
            return new VrCoreInfo(gvrVersion, VrCoreCompatibility.VR_READY);
        } catch (VrCoreNotAvailableException e) {
            Log.i(TAG, "Unable to find VrCore.");
            // Old versions of VrCore are not integrated with the sdk library version check and will
            // trigger this exception even though VrCore is installed.
            // Double check package manager to make sure we are not telling user to install
            // when it should just be an update.
            if (PackageUtils.getPackageVersion(
                        ContextUtils.getApplicationContext(), VR_CORE_PACKAGE_ID)
                    != -1) {
                return new VrCoreInfo(null, VrCoreCompatibility.VR_OUT_OF_DATE);
            }
            return new VrCoreInfo(null, VrCoreCompatibility.VR_NOT_AVAILABLE);
        } catch (SecurityException e) {
            Log.e(TAG, "Cannot query VrCore version: " + e.toString());
            return new VrCoreInfo(null, VrCoreCompatibility.VR_NOT_AVAILABLE);
        }
    }

    public int getVrCoreCompatibility() {
        return getVrCoreInfo().compatibility;
    }

    public long makeNativeVrCoreInfo() {
        return getVrCoreInfo().makeNativeVrCoreInfo();
    }
}
