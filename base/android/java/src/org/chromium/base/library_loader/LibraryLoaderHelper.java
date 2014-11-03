// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


package org.chromium.base.library_loader;

import android.content.Context;
import android.os.Build;
import android.util.Log;

import java.io.BufferedOutputStream;
import java.io.Closeable;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;

/**
 * Class representing an exception which occured during the unpacking process.
 */
class UnpackingException extends Exception {
    public UnpackingException(String message, Throwable cause) {
        super(message, cause);
    }

    public UnpackingException(String message) {
        super(message);
    }
}

/**
 * The class provides helper functions to extract native libraries from APK,
 * and load libraries from there.
 *
 * The class should be package-visible only, but made public for testing
 * purpose.
 */
public class LibraryLoaderHelper {
    private static final String TAG = "LibraryLoaderHelper";

    // Workaround and fallback directories.
    // TODO(petrcermak): Merge the directories once refactored.
    public static final String PACKAGE_MANAGER_WORKAROUND_DIR = "lib";
    public static final String LOAD_FROM_APK_FALLBACK_DIR = "fallback";

    private static final int BUFFER_SIZE = 16384;

    /**
     * One-way switch becomes true if native libraries were unpacked
     * from APK.
     */
    private static boolean sLibrariesWereUnpacked = false;

    /**
     * Loads native libraries using workaround only, skip the library in system
     * lib path. The method exists only for testing purpose.
     * Caller must ensure thread safety of this method.
     * @param context
     */
    public static boolean loadNativeLibrariesUsingWorkaroundForTesting(Context context) {
        // Although tryLoadLibraryUsingWorkaround might be called multiple times,
        // libraries should only be unpacked once, this is guaranteed by
        // sLibrariesWereUnpacked.
        for (String library : NativeLibraries.LIBRARIES) {
            if (!tryLoadLibraryUsingWorkaround(context, library)) {
                return false;
            }
        }
        return true;
    }

    /**
     * Try to load a native library using a workaround of
     *   http://b/13216167.
     *
     * Workaround for b/13216167 was adapted from code in
     * https://googleplex-android-review.git.corp.google.com/#/c/433061
     *
     * More details about http://b/13216167:
     *   PackageManager may fail to update shared library.
     *
     * Native library directory in an updated package is a symbolic link
     * to a directory in /data/app-lib/<package name>, for example:
     * /data/data/com.android.chrome/lib -> /data/app-lib/com.android.chrome[-1].
     * When updating the application, the PackageManager create a new directory,
     * e.g., /data/app-lib/com.android.chrome-2, and remove the old symlink and
     * recreate one to the new directory. However, on some devices (e.g. Sony Xperia),
     * the symlink was updated, but fails to extract new native libraries from
     * the new apk.
     *
     * We make the following changes to alleviate the issue:
     *  1) name the native library with apk version code, e.g.,
     *     libchrome.1750.136.so, 1750.136 is Chrome version number;
     *  2) first try to load the library using System.loadLibrary,
     *     if that failed due to the library file was not found,
     *     search the named library in a /data/data/com.android.chrome/app_lib
     *     directory. Because of change 1), each version has a different native
     *     library name, so avoid mistakenly using the old native library.
     *
     *  If named library is not in /data/data/com.android.chrome/app_lib directory,
     *  extract native libraries from apk and cache in the directory.
     *
     * This function doesn't throw UnsatisfiedLinkError, the caller needs to
     * check the return value.
     */
    static boolean tryLoadLibraryUsingWorkaround(Context context, String library) {
        assert context != null;
        String libName = System.mapLibraryName(library);
        File libFile = new File(getLibDir(context, PACKAGE_MANAGER_WORKAROUND_DIR), libName);
        if (!libFile.exists() && !unpackWorkaroundLibrariesOnce(context)) {
            return false;
        }
        try {
            System.load(libFile.getAbsolutePath());
            return true;
        } catch (UnsatisfiedLinkError e) {
            return false;
        }
    }

    /**
     * Returns the directory for holding extracted native libraries.
     * It may create the directory if it doesn't exist.
     *
     * @param context The context the code is running.
     * @param dirName The name of the directory containing the libraries.
     * @return The directory file object.
     */
    public static File getLibDir(Context context, String dirName) {
        return context.getDir(dirName, Context.MODE_PRIVATE);
    }

    @SuppressWarnings("deprecation")
    private static String getJniNameInApk(String libName) {
        // TODO(aurimas): Build.CPU_ABI has been deprecated. Replace it when final L SDK is public.
        return "lib/" + Build.CPU_ABI + "/" + libName;
    }

    /**
     * Unpack native libraries from the APK file. The method is supposed to
     * be called only once. It deletes existing files in unpacked directory
     * before unpacking.
     *
     * @param context
     * @return true when unpacking was successful, false when failed or called
     *         more than once.
     */
    private static boolean unpackWorkaroundLibrariesOnce(Context context) {
        if (sLibrariesWereUnpacked) {
            return false;
        }
        sLibrariesWereUnpacked = true;

        deleteLibrariesSynchronously(context, PACKAGE_MANAGER_WORKAROUND_DIR);
        File libDir = getLibDir(context, PACKAGE_MANAGER_WORKAROUND_DIR);

        try {
            Map<String, File> dstFiles = new HashMap<String, File>();
            for (String library : NativeLibraries.LIBRARIES) {
                String libName = System.mapLibraryName(library);
                String pathInZipFile = getJniNameInApk(libName);
                dstFiles.put(pathInZipFile, new File(libDir, libName));
            }
            unpackLibraries(context, dstFiles);
            return true;
        } catch (UnpackingException e) {
            Log.e(TAG, "Failed to unpack native libraries", e);
            deleteLibrariesSynchronously(context, PACKAGE_MANAGER_WORKAROUND_DIR);
            return false;
        }
    }

    /**
     * Delete libraries and their directory synchronously.
     * For testing purpose only.
     *
     * @param context
     */
    public static void deleteLibrariesSynchronously(Context context, String dirName) {
        File libDir = getLibDir(context, dirName);
        deleteObsoleteLibraries(libDir, Collections.<File>emptyList());
    }

    /**
     * Delete libraries and their directory asynchronously.
     * The actual deletion is done in a background thread.
     *
     * @param context
     */
    static void deleteLibrariesAsynchronously(
            final Context context, final String dirName) {
        // Child process should not reach here.
        new Thread() {
            @Override
            public void run() {
                deleteLibrariesSynchronously(context, dirName);
            }
        }.start();
    }

    /**
     * Copy a library from a zip file to the application's private directory.
     * This is used as a fallback when we are unable to load the library
     * directly from the APK file (crbug.com/390618).
     *
     * @param context The context the code is running in.
     * @param library Library name.
     * @return name of the fallback copy of the library.
     */
    public static String buildFallbackLibrary(Context context, String library) {
        try {
            String libName = System.mapLibraryName(library);
            File fallbackLibDir = getLibDir(context, LOAD_FROM_APK_FALLBACK_DIR);
            File fallbackLibFile = new File(fallbackLibDir, libName);
            String pathInZipFile = Linker.getLibraryFilePathInZipFile(libName);
            Map<String, File> dstFiles = Collections.singletonMap(pathInZipFile, fallbackLibFile);

            deleteObsoleteLibraries(fallbackLibDir, dstFiles.values());
            unpackLibraries(context, dstFiles);

            return fallbackLibFile.getAbsolutePath();
        } catch (Exception e) {
            String errorMessage = "Unable to load fallback for library " + library
                    + " (" + (e.getMessage() == null ? e.toString() : e.getMessage()) + ")";
            Log.e(TAG, errorMessage, e);
            throw new UnsatisfiedLinkError(errorMessage);
        }
    }

    // Delete obsolete libraries from a library folder.
    private static void deleteObsoleteLibraries(File libDir, Collection<File> keptFiles) {
        try {
            // Build a list of libraries that should NOT be deleted.
            Set<String> keptFileNames = new HashSet<String>();
            for (File k : keptFiles) {
                keptFileNames.add(k.getName());
            }

            // Delete the obsolete libraries.
            Log.i(TAG, "Deleting obsolete libraries in " + libDir.getPath());
            File[] files = libDir.listFiles();
            if (files != null) {
                for (File f : files) {
                    if (!keptFileNames.contains(f.getName())) {
                        delete(f);
                    }
                }
            } else {
                Log.e(TAG, "Failed to list files in " + libDir.getPath());
            }

            // Delete the folder if no libraries were kept.
            if (keptFileNames.isEmpty()) {
                delete(libDir);
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to remove obsolete libraries from " + libDir.getPath());
        }
    }

    // Unpack libraries from a zip file to the file system.
    private static void unpackLibraries(Context context,
            Map<String, File> dstFiles) throws UnpackingException {
        String zipFilePath = context.getApplicationInfo().sourceDir;
        Log.i(TAG, "Opening zip file " + zipFilePath);
        File zipFile = new File(zipFilePath);
        ZipFile zipArchive = openZipFile(zipFile);

        try {
            for (Entry<String, File> d : dstFiles.entrySet()) {
                String pathInZipFile = d.getKey();
                File dstFile = d.getValue();
                Log.i(TAG, "Unpacking " + pathInZipFile
                        + " to " + dstFile.getAbsolutePath());
                ZipEntry packedLib = zipArchive.getEntry(pathInZipFile);

                if (needToUnpackLibrary(zipFile, packedLib, dstFile)) {
                    unpackLibraryFromZipFile(zipArchive, packedLib, dstFile);
                    setLibraryFilePermissions(dstFile);
                }
            }
        } finally {
            closeZipFile(zipArchive);
        }
    }

    // Open a zip file.
    private static ZipFile openZipFile(File zipFile) throws UnpackingException {
        try {
            return new ZipFile(zipFile);
        } catch (ZipException e) {
            throw new UnpackingException("Failed to open zip file " + zipFile.getPath());
        } catch (IOException e) {
            throw new UnpackingException("Failed to open zip file " + zipFile.getPath());
        }
    }

    // Determine whether it is necessary to unpack a library from a zip file.
    private static boolean needToUnpackLibrary(
            File zipFile, ZipEntry packedLib, File dstFile) {
        // Check if the fallback library already exists.
        if (!dstFile.exists()) {
            Log.i(TAG, "File " + dstFile.getPath() + " does not exist yet");
            return true;
        }

        // Check last modification dates.
        long zipTime = zipFile.lastModified();
        long fallbackLibTime = dstFile.lastModified();
        if (zipTime > fallbackLibTime) {
            Log.i(TAG, "Not using existing fallback file because "
                    + "the APK file " + zipFile.getPath()
                    + " (timestamp=" + zipTime + ") is newer than "
                    + "the fallback library " + dstFile.getPath()
                    + "(timestamp=" + fallbackLibTime + ")");
            return true;
        }

        // Check file sizes.
        long packedLibSize = packedLib.getSize();
        long fallbackLibSize = dstFile.length();
        if (fallbackLibSize != packedLibSize) {
            Log.i(TAG, "Not using existing fallback file because "
                    + "the library in the APK " + zipFile.getPath()
                    + " (" + packedLibSize + "B) has a different size than "
                    + "the fallback library " + dstFile.getPath()
                    + "(" + fallbackLibSize + "B)");
            return true;
        }

        Log.i(TAG, "Reusing existing file " + dstFile.getPath());
        return false;
    }

    // Unpack a library from a zip file to the filesystem.
    private static void unpackLibraryFromZipFile(ZipFile zipArchive, ZipEntry packedLib,
            File dstFile) throws UnpackingException {
        // Open input stream for the library file inside the zip file.
        InputStream in;
        try {
            in = zipArchive.getInputStream(packedLib);
        } catch (IOException e) {
            throw new UnpackingException(
                    "IO exception when locating library in the zip file", e);
        }

        // Ensure that the input stream is closed at the end.
        try {
            // Delete existing file if it exists.
            if (dstFile.exists()) {
                Log.i(TAG, "Deleting existing unpacked library file " + dstFile.getPath());
                if (!dstFile.delete()) {
                    throw new UnpackingException(
                            "Failed to delete existing unpacked library file " + dstFile.getPath());
                }
            }

            // Ensure that the library folder exists. Since this is added
            // for increased robustness, we log errors and carry on.
            try {
                dstFile.getParentFile().mkdirs();
            } catch (Exception e) {
                Log.e(TAG, "Failed to make library folder", e);
            }

            // Create the destination file.
            try {
                if (!dstFile.createNewFile()) {
                    throw new UnpackingException("existing unpacked library file was not deleted");
                }
            } catch (IOException e) {
                throw new UnpackingException("failed to create unpacked library file", e);
            }

            // Open the output stream for the destination file.
            OutputStream out;
            try {
                out = new BufferedOutputStream(new FileOutputStream(dstFile));
            } catch (FileNotFoundException e) {
                throw new UnpackingException(
                        "failed to open output stream for unpacked library file", e);
            }

            // Ensure that the output stream is closed at the end.
            try {
                // Copy the library from the zip file to the destination file.
                Log.i(TAG, "Copying " + packedLib.getName() + " from " + zipArchive.getName()
                        + " to " + dstFile.getPath());
                byte[] buffer = new byte[BUFFER_SIZE];
                int len;
                while ((len = in.read(buffer)) != -1) {
                    out.write(buffer, 0, len);
                }
            } catch (IOException e) {
                throw new UnpackingException(
                        "failed to copy the library from the zip file", e);
            } finally {
                close(out, "output stream");
            }
        } finally {
            close(in, "input stream");
        }
    }

    // Set up library file permissions.
    private static void setLibraryFilePermissions(File libFile) {
        // Change permission to rwxr-xr-x
        Log.i(TAG, "Setting file permissions for " + libFile.getPath());
        if (!libFile.setReadable(/* readable */ true, /* ownerOnly */ false)) {
            Log.e(TAG, "failed to chmod a+r the temporary file");
        }
        if (!libFile.setExecutable(/* executable */ true, /* ownerOnly */ false)) {
            Log.e(TAG, "failed to chmod a+x the temporary file");
        }
        if (!libFile.setWritable(/* writable */ true)) {
            Log.e(TAG, "failed to chmod +w the temporary file");
        }
    }

    // Close a closable and log a warning if it fails.
    private static void close(Closeable closeable, String name) {
        try {
            closeable.close();
        } catch (IOException e) {
            // Warn and ignore.
            Log.w(TAG, "IO exception when closing " + name, e);
        }
    }

    // Close a zip file and log a warning if it fails.
    // This needs to be a separate method because ZipFile is not Closeable in
    // Java 6 (used on some older devices).
    private static void closeZipFile(ZipFile file) {
        try {
            file.close();
        } catch (IOException e) {
            // Warn and ignore.
            Log.w(TAG, "IO exception when closing zip file", e);
        }
    }

    // Delete a file and log it.
    private static void delete(File file) {
        if (file.delete()) {
            Log.i(TAG, "Deleted " + file.getPath());
        } else {
            Log.w(TAG, "Failed to delete " + file.getPath());
        }
    }
}
