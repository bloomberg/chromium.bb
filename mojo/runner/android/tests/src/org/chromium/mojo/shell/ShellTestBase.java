// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.shell;

import android.content.Context;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.Log;

import java.io.File;
import java.io.IOException;

/**
 * Helper method for ShellTestBase.
 */
@JNINamespace("mojo::runner::test")
public class ShellTestBase {
    private static final String TAG = "ShellTestBase";

    /**
     * Extracts the mojo applications from the apk assets and returns the directory where they are.
     */
    @CalledByNative
    private static String extractMojoApplications(Context context) throws IOException {
        File cachedAppsDir = FileHelper.getCachedAppsDir(context);
        try {
            FileHelper.prepareDirectoryForAssets(context, cachedAppsDir);
            for (String assetPath : FileHelper.getAssetsList(context)) {
                FileHelper.extractFromAssets(
                        context, assetPath, cachedAppsDir, FileHelper.FileType.PERMANENT);
            }
        } catch (Exception e) {
            Log.e(TAG, "ShellTestBase initialization failed.", e);
            throw new RuntimeException(e);
        }
        return cachedAppsDir.getAbsolutePath();
    }
}
