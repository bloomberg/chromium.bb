// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Intent;

import org.chromium.base.library_loader.LibraryProcessType;

/**
 * Allows specifying the package name for looking up child services
 * configuration and classes into (if it differs from the application
 * package name, like in the case of Android WebView). Also allows
 * specifying additional child service binging flags.
 */
public class ChildProcessCreationParams {
    private final String mPackageName;
    private final boolean mIsExternalService;
    private final int mLibraryProcessType;
    private static final String EXTRA_LIBRARY_PROCESS_TYPE =
            "org.chromium.content.common.child_service_params.library_process_type";

    private static volatile ChildProcessCreationParams sChildProcessCreationParams;

    public static void set(ChildProcessCreationParams params) {
        sChildProcessCreationParams = params;
    }

    public static ChildProcessCreationParams get()  {
        return sChildProcessCreationParams;
    }

    public ChildProcessCreationParams(String packageName, boolean isExternalService,
            int libraryProcessType) {
        mPackageName = packageName;
        mIsExternalService = isExternalService;
        mLibraryProcessType = libraryProcessType;
    }

    public ChildProcessCreationParams copy() {
        return new ChildProcessCreationParams(mPackageName, mIsExternalService,
                mLibraryProcessType);
    }

    public String getPackageName() {
        return mPackageName;
    }

    public boolean getIsExternalService() {
        return mIsExternalService;
    }

    public int getLibraryProcessType() {
        return mLibraryProcessType;
    }

    public void addIntentExtras(Intent intent) {
        intent.putExtra(EXTRA_LIBRARY_PROCESS_TYPE, mLibraryProcessType);
    }

    public static int getLibraryProcessType(Intent intent) {
        return intent.getIntExtra(EXTRA_LIBRARY_PROCESS_TYPE,
                LibraryProcessType.PROCESS_CHILD);
    }
}
