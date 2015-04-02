// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.shell;

import android.content.Context;
import android.content.res.AssetManager;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

import java.io.File;
import java.io.IOException;

/**
 * Helper method for ShellTestBase.
 */
@JNINamespace("mojo::shell::test")
public class ShellTestBase {
    // Directory where applications bundled with the tests will be extracted.
    private static final String TEST_APP_DIRECTORY = "test_apps";

    /**
     * Extracts the mojo applications from the apk assets and returns the directory where they are.
     */
    @CalledByNative
    private static String extractMojoApplications(Context context) throws IOException {
        final File outputDirectory = context.getDir(TEST_APP_DIRECTORY, Context.MODE_PRIVATE);

        AssetManager manager = context.getResources().getAssets();
        for (String asset : manager.list("")) {
            if (asset.endsWith(".mojo")) {
                FileHelper.extractFromAssets(context, asset, outputDirectory, false);
            }
        }

        return outputDirectory.getAbsolutePath();
    }
}
