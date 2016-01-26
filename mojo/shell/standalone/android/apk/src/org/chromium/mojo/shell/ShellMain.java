// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.shell;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A placeholder class to call native functions.
 **/
@JNINamespace("mojo::shell")
public class ShellMain {
    private static final String TAG = "ShellMain";

    // The key to the library to run in forked processes when running multi-process.
    private static final String MOJO_LIB_KEY = "mojo_lib";

    /**
     * A guard flag for calling nativeInit() only once.
     **/
    private static boolean sInitialized = false;

    /**
     * Initializes the native system.
     **/
    public static void ensureInitialized(Context context, String[] parameters) {
        if (sInitialized) return;
        File cachedAppsDir = getCachedAppsDir(context);
        try {
            final File timestamp = FileHelper.prepareDirectoryForAssets(context, cachedAppsDir);
            for (String assetPath : FileHelper.getAssetsList(context)) {
                FileHelper.extractFromAssets(
                        context, assetPath, cachedAppsDir, FileHelper.FileType.PERMANENT);
            }
            ApplicationInfo ai = context.getPackageManager().getApplicationInfo(
                    context.getPackageName(), PackageManager.GET_META_DATA);
            Bundle bundle = ai.metaData;
            String mojo_lib = bundle.getString(MOJO_LIB_KEY);

            FileHelper.createTimestampIfNecessary(timestamp);
            File mojoShell = new File(context.getApplicationInfo().nativeLibraryDir, mojo_lib);

            List<String> parametersList = new ArrayList<String>();
            // Program name.
            if (parameters != null) {
                parametersList.addAll(Arrays.asList(parameters));
            }

            ContextUtils.initApplicationContext(context.getApplicationContext());
            nativeInit(context, mojoShell.getAbsolutePath(),
                    parametersList.toArray(new String[parametersList.size()]),
                    cachedAppsDir.getAbsolutePath(), getTmpDir(context).getAbsolutePath());
            sInitialized = true;
        } catch (Exception e) {
            Log.e(TAG, "ShellMain initialization failed.", e);
            throw new RuntimeException(e);
        }
    }

    /**
     * Starts the specified application in the specified context.
     **/
    public static void start() {
        nativeStart();
    }

    /**
     * Adds the given URL to the set of mojo applications to run on start.
     */
    static void addApplicationURL(String url) {
        nativeAddApplicationURL(url);
    }

    static File getCachedAppsDir(Context context) {
        return FileHelper.getCachedAppsDir(context);
    }

    private static File getTmpDir(Context context) {
        return new File(context.getCacheDir(), "tmp");
    }

    @CalledByNative
    private static void finishActivity(Activity activity) {
        activity.finish();
    }

    /**
     * Initializes the native system. This API should be called only once per process.
     **/
    private static native void nativeInit(Context context, String mojoShellPath,
            String[] parameters, String cachedAppsDirectory, String tmpDir);

    private static native void nativeStart();

    private static native void nativeAddApplicationURL(String url);
}
