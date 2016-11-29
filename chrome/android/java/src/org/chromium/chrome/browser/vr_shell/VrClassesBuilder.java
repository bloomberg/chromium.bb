// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

/**
 * Abstracts away the VrClassesBuilderImpl class, which may or may not be present at runtime
 * depending on compile flags.
 */
public interface VrClassesBuilder {
    /**
     * Creates a NonPresentingGvrContextImpl instance.
     */
    public NonPresentingGvrContext createNonPresentingGvrContext();

    /**
     * Creates a VrShellImpl instance.
     */
    public VrShell createVrShell();

    /**
     * Creates a VrDaydreamApImpl instance.
     */
    public VrDaydreamApi createVrDaydreamApi();

    /**
    * Creates a VrCoreVersionCheckerImpl instance.
    */
    public VrCoreVersionChecker createVrCoreVersionChecker();
}
