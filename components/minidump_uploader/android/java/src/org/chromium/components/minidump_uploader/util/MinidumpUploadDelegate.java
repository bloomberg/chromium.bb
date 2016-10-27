// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader.util;

import android.content.Context;
import android.support.annotation.StringDef;

/**
 * Interface giving clients of the minidump uploader component the ability to control and log
 * different stages of the uploading.
 */
public interface MinidumpUploadDelegate {
    @StringDef({BROWSER, RENDERER, GPU, OTHER})
    public @interface ProcessType {}
    static final String BROWSER = "Browser";
    static final String RENDERER = "Renderer";
    static final String GPU = "GPU";
    static final String OTHER = "Other";

    static final String[] CRASH_TYPES = {BROWSER, RENDERER, GPU, OTHER};

    /**
     * Called when a minidump is uploaded successfully.
     * @param context is the context which we should use to fetch information such as shared
     * preferences.
     * @param crashType is the type of crash represented by the minidump that was uploaded.
     */
    void onSuccessfulUpload(Context context, @ProcessType String crashType);

    /**
     * Called when we fail to upload a minidump too many times.
     * @param context is the context which we should use to fetch information such as shared
     * preferences.
     * @param crashType is the type of crash represented by the minidump we failed to upload.
     */
    void onMaxedOutUploadFailures(Context context, @ProcessType String crashType);

    /**
     * @return A permission manager determining whether a minidump can be uploaded at certain times.
     */
    CrashReportingPermissionManager getCrashReportingPermissionManager();
}
