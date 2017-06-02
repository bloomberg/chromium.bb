// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import com.google.vr.ndk.base.Version;

import org.chromium.base.annotations.JNINamespace;

/**
 * Container class to provide the version and the compatibility with Chromium of the installed
 * VrCore.
 */
@JNINamespace("vr_shell")
public class VrCoreInfo {
    public final Version gvrVersion;
    @VrCoreCompatibility
    public final int compatibility;

    public VrCoreInfo(Version gvrVersion, int compatibility) {
        this.gvrVersion = gvrVersion;
        this.compatibility = compatibility;
    }

    public long makeNativeVrCoreInfo() {
        return (gvrVersion == null) ? nativeInit(0, 0, 0, compatibility)
                                    : nativeInit(gvrVersion.majorVersion, gvrVersion.minorVersion,
                                              gvrVersion.patchVersion, compatibility);
    }

    private native long nativeInit(
            int majorVersion, int minorVersion, int patchVersion, int compatibility);
}
