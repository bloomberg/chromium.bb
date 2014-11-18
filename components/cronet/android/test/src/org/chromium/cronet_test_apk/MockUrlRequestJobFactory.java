// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_test_apk;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import org.chromium.base.JNINamespace;
import org.chromium.base.PathUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * Helper class to set up url interceptors for testing purposes.
 */
@JNINamespace("cronet")
public final class MockUrlRequestJobFactory {
    private static final String TAG = "MockUrlRequestJobFactory";
    // Name of the asset directory in which test files are stored.
    private static final String TEST_FILE_ASSET_PATH = "test";

    // Application context.
    private final Context mContext;

    // An array of file names that are installed on the device.
    private List<String> mInstalledFiles;

    // Indicates whether setUp() has completed.
    private boolean mInstallCompleted;

    enum FailurePhase {
        START,
        READ_ASYNC,
        READ_SYNC,
    };

    /**
     * Constructs a MockUrlRequestJobFactory.
     * @param context application context.
     */
    public MockUrlRequestJobFactory(Context context) {
        mContext = context;
        mInstalledFiles = new ArrayList<String>();
        mInstallCompleted = false;
    }

    /**
     * Sets up test environment for mock URLRequestJobs.
     * @param context application context.
     */
    public void setUp() {
        nativeAddUrlInterceptors();
        installTestFiles();
    }

    /**
     * Tears down test environment for mock URLRequestJobs.
     * @param context application context.
     */
    public void tearDown() {
        // TODO(xunjieli): clear url interceptors.
        uninstallTestFiles();
    }

    /**
     * Constructs a mock URL.
     *
     * @param path path to a mock file.
     */
    public String getMockUrl(String path) {
        if (!mInstallCompleted) {
            throw new IllegalStateException("installation has not completed");
        }
        return nativeGetMockUrl(path);
    }

    /**
     * Constructs a mock URL that hangs or fails at certain phase.
     *
     * @param path path to a mock file.
     * @param phase at which request fails.
     * @param netError reported by UrlRequestJob. Passing -1, results in hang.
     */
    public String getMockUrlWithFailure(String path, FailurePhase phase,
            int netError) {
        if (!mInstallCompleted) {
            throw new IllegalStateException("installation has not completed");
        }
        return nativeGetMockUrlWithFailure(path, phase.ordinal(), netError);
    }

    /**
     * Installs test files that are included in assets.
     */
    private void installTestFiles() {
        String toPath = PathUtils.getDataDirectory(mContext);
        AssetManager assetManager = mContext.getAssets();
        try {
            String[] files = assetManager.list(TEST_FILE_ASSET_PATH);
            Log.i(TAG, "Begin loading " + files.length + " test files.");
            for (String fileName : files) {
                Log.i(TAG, "Loading " + fileName);
                String destFilePath = toPath + "/" + fileName;
                if (copyTestFile(assetManager,
                                 TEST_FILE_ASSET_PATH + "/" + fileName,
                                 destFilePath)) {
                    mInstalledFiles.add(destFilePath);
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        mInstallCompleted = true;
    }

    /**
     * Uninstalls all test files that have been installed.
     */
    private void uninstallTestFiles() {
        if (!mInstallCompleted) {
            throw new IllegalStateException("installation has not completed");
        }
        for (String fileName : mInstalledFiles) {
            File file = new File(fileName);
            if (!file.delete()) {
                Log.e(TAG, "deleting " + fileName + " failed.");
            }
        }
    }

    /**
     * Copies a file from assets to the device's file system.
     * @param assetManager AssetManager of the application.
     * @param srcFilePath the source file path in assets.
     * @param destFilePath the destination file path.
     */
    private static boolean copyTestFile(AssetManager assetManager,
                                        String srcFilePath,
                                        String destFilePath) {
        OutputStream out;
        try {
            File destFile = new File(destFilePath);
            if (destFile.exists()) {
                throw new IllegalStateException(srcFilePath
                        + " already exists");
            }
            out = new FileOutputStream(destFilePath);
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
        try {
            InputStream in = assetManager.open(srcFilePath);

            byte[] buffer = new byte[1024];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            in.close();
            out.flush();
            out.close();
            return true;
        } catch (Exception e) {
            try {
                out.close();
            } catch (Exception closeException) {
                closeException.printStackTrace();
            }
            e.printStackTrace();
            return false;
        }
    }

    private static native void nativeAddUrlInterceptors();

    private static native String nativeGetMockUrl(String path);

    private static native String nativeGetMockUrlWithFailure(String path,
            int phase, int netError);
}
