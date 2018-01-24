// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.os.Bundle;

import org.chromium.base.library_loader.LibraryProcessType;

/**
 * Allows specifying the package name for looking up child services
 * configuration and classes into (if it differs from the application package
 * name, like in the case of Android WebView). Also allows specifying additional
 * child service binging flags.
 */
public class ChildProcessCreationParams {
    private static final String EXTRA_LIBRARY_PROCESS_TYPE =
            "org.chromium.content.common.child_service_params.library_process_type";

    private static ChildProcessCreationParams sParams;

    /** Registers default params. This should be called once on start up. */
    public static void registerDefault(ChildProcessCreationParams params) {
        assert sParams == null;
        sParams = params;
    }

    public static ChildProcessCreationParams getDefault() {
        return sParams;
    }

    // Members should all be immutable to avoid worrying about thread safety.
    private final String mPackageNameForSandboxedService;
    private final boolean mIsSandboxedServiceExternal;
    private final int mLibraryProcessType;
    private final boolean mBindToCallerCheck;
    // Use only the explicit WebContents.setImportance signal, and ignore other implicit
    // signals in content.
    private final boolean mIgnoreVisibilityForImportance;

    public ChildProcessCreationParams(String packageNameForSandboxedService,
            boolean isExternalSandboxedService, int libraryProcessType, boolean bindToCallerCheck,
            boolean ignoreVisibilityForImportance) {
        mPackageNameForSandboxedService = packageNameForSandboxedService;
        mIsSandboxedServiceExternal = isExternalSandboxedService;
        mLibraryProcessType = libraryProcessType;
        mBindToCallerCheck = bindToCallerCheck;
        mIgnoreVisibilityForImportance = ignoreVisibilityForImportance;
    }

    public String getPackageNameForSandboxedService() {
        return mPackageNameForSandboxedService;
    }

    public boolean getIsSandboxedServiceExternal() {
        return mIsSandboxedServiceExternal;
    }

    public boolean getBindToCallerCheck() {
        return mBindToCallerCheck;
    }

    public boolean getIgnoreVisibilityForImportance() {
        return mIgnoreVisibilityForImportance;
    }

    public void addIntentExtras(Bundle extras) {
        extras.putInt(EXTRA_LIBRARY_PROCESS_TYPE, mLibraryProcessType);
    }

    public static int getLibraryProcessType(Bundle extras) {
        return extras.getInt(EXTRA_LIBRARY_PROCESS_TYPE, LibraryProcessType.PROCESS_CHILD);
    }
}
