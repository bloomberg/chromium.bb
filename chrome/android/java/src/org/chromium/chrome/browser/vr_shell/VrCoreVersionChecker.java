// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

/**
 * Abstracts away the VrCoreVersionCheckerImpl class, which may or may not be present at runtime
 * depending on compile flags.
 */
public interface VrCoreVersionChecker {
    public static final String VR_CORE_PACKAGE_ID = "com.google.vr.vrcore";

    /**
     * Returns the version of VrCore (if it is installed) and the compatibility of VrCore with
     * Chrome.
     */
    VrCoreInfo getVrCoreInfo();
}
