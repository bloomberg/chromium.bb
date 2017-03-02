// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.File;

/**
 * Interface for embedder-specific implementations for uploading minidumps.
 */
public interface MinidumpUploaderDelegate {
    /**
     * Creates the directory in which the embedder will store its minidumps.
     * @return A reference to the created directory, or null if the creation failed.
     */
    File createCrashDir();

    /**
     * Creates the permission manager used to evaluate whether uploading should be allowed.
     * @return The permission manager.
     */
    CrashReportingPermissionManager createCrashReportingPermissionManager();

    /**
     * Performs any pre-work necessary for uploading minidumps, then calls the {@param startUploads}
     * continuation to initiate uploading the minidumps.
     * @param startUploads The continuation to call once any necessary pre-work is completed.
     */
    void prepareToUploadMinidumps(Runnable startUploads);
}
