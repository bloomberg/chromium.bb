// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.shell;

import android.content.Context;

import dalvik.system.DexClassLoader;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.File;
import java.io.FilenameFilter;
import java.io.IOException;
import java.lang.reflect.Constructor;

/**
 * Content handler for archives containing native libraries bundled with Java code.
 * <p>
 * TODO(ppi): create a separate instance for each application being bootstrapped to keep track of
 * the temporary files and clean them up once the execution finishes.
 */
@JNINamespace("mojo::shell")
public class AndroidHandler {
    private static final String TAG = "AndroidHandler";

    // Bootstrap native and java libraries are packaged with the MojoShell APK as assets.
    private static final String BOOTSTRAP_JAVA_LIBRARY = "bootstrap_java.dex.jar";
    private static final String BOOTSTRAP_NATIVE_LIBRARY = "libbootstrap.so";
    // Name of the bootstrapping runnable shipped in the packaged Java library.
    private static final String BOOTSTRAP_CLASS = "org.chromium.mojo.shell.Bootstrap";

    // File extensions used to identify application libraries in the provided archive.
    private static final String JAVA_LIBRARY_SUFFIX = ".dex.jar";
    private static final String NATIVE_LIBRARY_SUFFIX = ".so";
    // Filename sections used for naming temporary files holding application files.
    private static final String ARCHIVE_PREFIX = "archive";
    private static final String ARCHIVE_SUFFIX = ".zip";

    // Directories used to hold temporary files. These are cleared when clearTemporaryFiles() is
    // called.
    private static final String DEX_OUTPUT_DIRECTORY = "dex_output";
    private static final String APP_DIRECTORY = "applications";
    private static final String ASSET_DIRECTORY = "assets";

    private static final String INTERNAL_DIRECTORY = "internal";

    /**
     * Deletes directories holding the temporary files. This should be called early on shell startup
     * to clean up after the previous run.
     */
    static void clearTemporaryFiles(Context context) {
        FileHelper.deleteRecursively(getDexOutputDir(context));
        FileHelper.deleteRecursively(getAppDir(context));
        FileHelper.deleteRecursively(getAssetDir(context));
    }

    /**
     * Returns the path at which the native part should save the application archive.
     */
    @CalledByNative
    private static String getNewTempArchivePath(Context context) throws IOException {
        return File.createTempFile(ARCHIVE_PREFIX, ARCHIVE_SUFFIX, getAppDir(context))
                .getAbsolutePath();
    }

    /**
     * Extracts and runs the application libraries contained by the indicated archive.
     * @param context the application context
     * @param archivePath the path of the archive containing the application to be run
     * @param handle handle to the shell to be passed to the native application. On the Java side
     *               this is opaque payload.
     * @param runApplicationPtr pointer to the function that will set the native thunks and call
     *                          into the application MojoMain. On the Java side this is opaque
     *                          payload.
     */
    @CalledByNative
    private static boolean bootstrap(
            Context context, String archivePath, int handle, long runApplicationPtr) {
        File bootstrapJavaLibrary;
        File bootstrapNativeLibrary;
        try {
            bootstrapJavaLibrary = FileHelper.extractFromAssets(context, BOOTSTRAP_JAVA_LIBRARY,
                    getAssetDir(context), FileHelper.FileType.TEMPORARY);
            bootstrapNativeLibrary = FileHelper.extractFromAssets(context, BOOTSTRAP_NATIVE_LIBRARY,
                    getAssetDir(context), FileHelper.FileType.TEMPORARY);
        } catch (Exception e) {
            Log.e(TAG, "Extraction of bootstrap files from assets failed.", e);
            return false;
        }

        File applicationJavaLibrary;
        File applicationNativeLibrary;
        try {
            File archive = new File(archivePath);
            applicationJavaLibrary =
                    FileHelper.extractFromArchive(archive, JAVA_LIBRARY_SUFFIX, getAppDir(context),
                            FileHelper.FileType.TEMPORARY, FileHelper.ArchiveType.NORMAL);
            applicationNativeLibrary = FileHelper.extractFromArchive(archive, NATIVE_LIBRARY_SUFFIX,
                    getAppDir(context), FileHelper.FileType.TEMPORARY,
                    FileHelper.ArchiveType.NORMAL);
        } catch (Exception e) {
            Log.e(TAG, "Extraction of application files from the archive failed.", e);
            return false;
        }

        return runApp(context, getDexOutputDir(context), applicationJavaLibrary,
                applicationNativeLibrary, bootstrapJavaLibrary, bootstrapNativeLibrary, handle,
                runApplicationPtr);
    }

    private static File findFileInDirectoryMatchingSuffix(
            File dir, final String suffix, final String ignore) {
        File[] matchingFiles = dir.listFiles(new FilenameFilter() {
            public boolean accept(File dir, String name) {
                return !name.equals(ignore) && !name.equals(BOOTSTRAP_JAVA_LIBRARY)
                        && !name.equals(BOOTSTRAP_NATIVE_LIBRARY) && name.endsWith(suffix);
            }
        });
        return matchingFiles != null && matchingFiles.length == 1 ? matchingFiles[0] : null;
    }

    /**
     * Extracts and runs a cached application.
     *
     * @param context the application context
     * @param archivePath the path of the archive containing the application to be run
     * @param appPathString path where the cached app's resources and other files are
     * @param handle handle to the shell to be passed to the native application. On the Java side
     *               this is opaque payload.
     * @param runApplicationPtr pointer to the function that will set the native thunks and call
     *                          into the application MojoMain. On the Java side this is opaque
     *                          payload.
     */
    @CalledByNative
    private static boolean bootstrapCachedApp(Context context, String archivePath,
            String appPathString, int handle, long runApplicationPtr) {
        final String appName = new File(appPathString).getName();
        final File internalDir = new File(new File(appPathString), INTERNAL_DIRECTORY);
        if (!internalDir.exists() && !internalDir.mkdirs()) {
            Log.e(TAG, "Unable to create output dir " + internalDir.getAbsolutePath());
            return false;
        }
        final File timestamp = FileHelper.prepareDirectoryForAssets(context, internalDir);
        // We make the bootstrap library have a unique name on disk as otherwise we end up sharing
        // bootstrap.so,
        // which doesn't work.
        // TODO(sky): figure out why android is caching the names.
        final String bootstrapNativeLibraryName = appName + "-" + BOOTSTRAP_NATIVE_LIBRARY;
        File bootstrapJavaLibrary = new File(internalDir, BOOTSTRAP_JAVA_LIBRARY);
        File bootstrapNativeLibrary = new File(internalDir, bootstrapNativeLibraryName);
        try {
            // Use the files on disk if we have them, if not extract from the archive.
            if (!bootstrapJavaLibrary.exists()) {
                bootstrapJavaLibrary = FileHelper.extractFromAssets(context, BOOTSTRAP_JAVA_LIBRARY,
                        internalDir, FileHelper.FileType.PERMANENT);
            }
            if (!bootstrapNativeLibrary.exists()) {
                final File extractedBootstrapNativeLibrary = FileHelper.extractFromAssets(context,
                        BOOTSTRAP_NATIVE_LIBRARY, internalDir, FileHelper.FileType.PERMANENT);
                if (!extractedBootstrapNativeLibrary.renameTo(bootstrapNativeLibrary)) {
                    Log.e(TAG, "Unable to rename bootstrap library "
                                    + bootstrapNativeLibrary.getAbsolutePath());
                    return false;
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Extraction of bootstrap files from assets failed.", e);
            return false;
        }

        // Because we allow the .so and .dex.jar to be anything we have to search in the internal
        // directory for matching files.
        // If we find one, we assume it's the one we want.
        File applicationJavaLibrary =
                findFileInDirectoryMatchingSuffix(internalDir, JAVA_LIBRARY_SUFFIX, "");
        File applicationNativeLibrary = findFileInDirectoryMatchingSuffix(
                internalDir, NATIVE_LIBRARY_SUFFIX, bootstrapNativeLibraryName);
        try {
            File archive = new File(archivePath);
            if (applicationJavaLibrary == null) {
                applicationJavaLibrary = FileHelper.extractFromArchive(archive, JAVA_LIBRARY_SUFFIX,
                        internalDir, FileHelper.FileType.PERMANENT,
                        FileHelper.ArchiveType.CONTENT_HANDLER);
            }
            if (applicationNativeLibrary == null) {
                applicationNativeLibrary = FileHelper.extractFromArchive(archive,
                        NATIVE_LIBRARY_SUFFIX, internalDir, FileHelper.FileType.PERMANENT,
                        FileHelper.ArchiveType.CONTENT_HANDLER);
            }
        } catch (Exception e) {
            Log.e(TAG, "Extraction of application files from the archive failed.", e);
            return false;
        }

        FileHelper.createTimestampIfNecessary(timestamp);

        return runApp(context, new File(internalDir, DEX_OUTPUT_DIRECTORY), applicationJavaLibrary,
                applicationNativeLibrary, bootstrapJavaLibrary, bootstrapNativeLibrary, handle,
                runApplicationPtr);
    }

    /**
     * Creates the class loader containing the bootstrap classes and runs it.
     *
     * @return true if successfully ran, false if encounteres some sort of error.
     */
    private static boolean runApp(Context context, File dexOutputDir, File applicationJavaLibrary,
            File applicationNativeLibrary, File bootstrapJavaLibrary, File bootstrapNativeLibrary,
            int handle, long runApplicationPtr) {
        final String dexPath = bootstrapJavaLibrary.getAbsolutePath() + File.pathSeparator
                + applicationJavaLibrary.getAbsolutePath();
        if (!dexOutputDir.exists() && !dexOutputDir.mkdirs()) {
            Log.e(TAG, "Unable to create dex output dir " + dexOutputDir.getAbsolutePath());
            return false;
        }
        // TODO(sky): third arg is a path, but appears to have no effect, figure out if this relates
        // to weird caching
        // logic mentioned above.
        DexClassLoader bootstrapLoader = new DexClassLoader(
                dexPath, dexOutputDir.getAbsolutePath(), null, ClassLoader.getSystemClassLoader());

        try {
            Class<?> loadedClass = bootstrapLoader.loadClass(BOOTSTRAP_CLASS);
            Class<? extends Runnable> bootstrapClass = loadedClass.asSubclass(Runnable.class);
            Constructor<? extends Runnable> constructor = bootstrapClass.getConstructor(
                    Context.class, File.class, File.class, Integer.class, Long.class);
            Runnable bootstrapRunnable = constructor.newInstance(context, bootstrapNativeLibrary,
                    applicationNativeLibrary, Integer.valueOf(handle),
                    Long.valueOf(runApplicationPtr));
            bootstrapRunnable.run();
        } catch (Throwable t) {
            Log.e(TAG, "Running Bootstrap failed.", t);
            return false;
        }
        return true;
    }

    @CalledByNative
    static String getCachedAppsDir(Context context) {
        return ShellMain.getCachedAppsDir(context).getAbsolutePath();
    }

    private static File getDexOutputDir(Context context) {
        return context.getDir(DEX_OUTPUT_DIRECTORY, Context.MODE_PRIVATE);
    }

    private static File getAppDir(Context context) {
        return context.getDir(APP_DIRECTORY, Context.MODE_PRIVATE);
    }

    private static File getAssetDir(Context context) {
        return context.getDir(ASSET_DIRECTORY, Context.MODE_PRIVATE);
    }
}
