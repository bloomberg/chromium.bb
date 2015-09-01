// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;

import java.io.File;
import java.util.List;

/**
 * Contains hooks for making Incremental Install work. Incremental Install is
 * where native libraries and (TODO) .dex files are pushed out-of-band to
 * external storage rather than packaged with the app.
 */
public final class IncrementalInstall {
    private static final String MANAGED_DIR_PREFIX = "/data/local/tmp/incremental-app-";

    public static void initialize(Context context) {
        File incrementalAppDir = new File(MANAGED_DIR_PREFIX + context.getPackageName());
        File incrementalLibDir = new File(incrementalAppDir, "lib");
        injectNativeLibDir(context.getClassLoader(), incrementalLibDir);
    }

    @SuppressWarnings("unchecked")
    private static void injectNativeLibDir(ClassLoader loader, File nativeLibDir) {
        try {
            Object dexPathList = Reflect.getField(loader, "pathList");
            Object currentDirs = Reflect.getField(dexPathList, "nativeLibraryDirectories");
            if (currentDirs instanceof List) {
                List<File> dirsAsList = (List<File>) currentDirs;
                dirsAsList.add(nativeLibDir);
            } else {
                File[] dirsAsArray = (File[]) currentDirs;
                File[] newDirs = new File[] {
                        nativeLibDir
                };
                Reflect.setField(dexPathList, "nativeLibraryDirectories",
                        Reflect.concatArrays(dirsAsArray, newDirs));
            }
        } catch (NoSuchFieldException e) {
            throw new RuntimeException(e);
        }
    }
}
