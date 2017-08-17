// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.StrictMode;

import org.junit.Assert;

import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.impl.JavaCronetProvider;

import java.io.File;
import java.net.URLStreamHandlerFactory;

// TODO(yolandyan): move this class to its test rule once JUnit4 migration is over
final class CronetTestCommon {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "cronet_test";

    private final CronetTestCommonCallback mCallback;

    private CronetTestFramework mCronetTestFramework;
    private URLStreamHandlerFactory mStreamHandlerFactory;

    // {@code true} when test is being run against system HttpURLConnection implementation.
    private boolean mTestingSystemHttpURLConnection;
    private boolean mTestingJavaImpl = false;
    private StrictMode.VmPolicy mOldVmPolicy;

    /**
     * Interface used by TestRule and TestBase class to implement so TestCommon class can
     * call the same API in either class despite implementation difference.
     *
     * In this case, TestCommon need to utilize test apk Context.
     */
    public interface CronetTestCommonCallback { Context getContextForTestCommon(); }

    CronetTestCommon(CronetTestCommonCallback callback) {
        mCallback = callback;
    }

    int getMaximumAvailableApiLevel() {
        // Prior to M59 the ApiVersion.getMaximumAvailableApiLevel API didn't exist
        if (ApiVersion.getCronetVersion().compareTo("59") < 0) {
            return 3;
        }
        return ApiVersion.getMaximumAvailableApiLevel();
    }

    /**
     * Returns the path for the test storage (http cache, QUIC server info).
     * NOTE: Does not ensure it exists; tests should use {@link #getTestStorage}.
     */
    private static String getTestStorageDirectory() {
        return PathUtils.getDataDirectory() + "/test_storage";
    }

    /**
     * Ensures test storage directory exists, i.e. creates one if it does not exist.
     */
    private static void ensureTestStorageExists() {
        File storage = new File(getTestStorageDirectory());
        if (!storage.exists()) {
            Assert.assertTrue(storage.mkdir());
        }
    }

    /**
     * Returns the path for the test storage (http cache, QUIC server info).
     * Also ensures it exists.
     */
    static String getTestStorage() {
        ensureTestStorageExists();
        return getTestStorageDirectory();
    }

    @SuppressFBWarnings("NP_NULL_ON_SOME_PATH_FROM_RETURN_VALUE")
    private static boolean recursiveDelete(File path) {
        if (path.isDirectory()) {
            for (File c : path.listFiles()) {
                if (!recursiveDelete(c)) {
                    return false;
                }
            }
        }
        return path.delete();
    }

    void setUp() throws Exception {
        System.loadLibrary("cronet_tests");
        ContextUtils.initApplicationContext(
                mCallback.getContextForTestCommon().getApplicationContext());
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        prepareTestStorage(mCallback.getContextForTestCommon());
        mOldVmPolicy = StrictMode.getVmPolicy();
        // Only enable StrictMode testing after leaks were fixed in crrev.com/475945
        if (getMaximumAvailableApiLevel() >= 7) {
            StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder()
                                           .detectLeakedClosableObjects()
                                           .penaltyLog()
                                           .penaltyDeath()
                                           .build());
        }
    }

    /**
     * Creates and returns {@link ExperimentalCronetEngine.Builder} that creates
     * Java (platform) based {@link CronetEngine.Builder}.
     *
     * @return the {@code CronetEngine.Builder} that builds Java-based {@code Cronet engine}.
     */
    ExperimentalCronetEngine.Builder createJavaEngineBuilder() {
        return (ExperimentalCronetEngine.Builder) new JavaCronetProvider(
                mCallback.getContextForTestCommon())
                .createBuilder();
    }

    @SuppressFBWarnings("DM_GC") // Used to trigger strictmode detecting leaked closeables
    void tearDown() throws Exception {
        try {
            // Run GC and finalizers a few times to pick up leaked closeables
            for (int i = 0; i < 10; i++) {
                System.gc();
                System.runFinalization();
            }
            System.gc();
            System.runFinalization();
        } finally {
            StrictMode.setVmPolicy(mOldVmPolicy);
        }
    }

    /**
     * Starts the CronetTest framework.
     */
    CronetTestFramework startCronetTestFramework() {
        mCronetTestFramework = new CronetTestFramework(mCallback.getContextForTestCommon());
        return mCronetTestFramework;
    }

    CronetTestFramework getCronetTestFramework() {
        return mCronetTestFramework;
    }

    void setTestingSystemHttpURLConnection(boolean value) {
        mTestingSystemHttpURLConnection = value;
    }

    /**
     * Returns {@code true} when test is being run against system HttpURLConnection implementation.
     */
    boolean testingSystemHttpURLConnection() {
        return mTestingSystemHttpURLConnection;
    }

    void setTestingJavaImpl(boolean value) {
        mTestingJavaImpl = value;
    }

    /**
     * Returns {@code true} when test is being run against the java implementation of CronetEngine.
     */
    boolean testingJavaImpl() {
        return mTestingJavaImpl;
    }

    void assertResponseEquals(UrlResponseInfo expected, UrlResponseInfo actual) {
        Assert.assertEquals(expected.getAllHeaders(), actual.getAllHeaders());
        Assert.assertEquals(expected.getAllHeadersAsList(), actual.getAllHeadersAsList());
        Assert.assertEquals(expected.getHttpStatusCode(), actual.getHttpStatusCode());
        Assert.assertEquals(expected.getHttpStatusText(), actual.getHttpStatusText());
        Assert.assertEquals(expected.getUrlChain(), actual.getUrlChain());
        Assert.assertEquals(expected.getUrl(), actual.getUrl());
        // Transferred bytes and proxy server are not supported in pure java
        if (!testingJavaImpl()) {
            Assert.assertEquals(expected.getReceivedByteCount(), actual.getReceivedByteCount());
            Assert.assertEquals(expected.getProxyServer(), actual.getProxyServer());
            // This is a place where behavior intentionally differs between native and java
            Assert.assertEquals(expected.getNegotiatedProtocol(), actual.getNegotiatedProtocol());
        }
    }

    static void assertContains(String expectedSubstring, String actualString) {
        Assert.assertNotNull(actualString);
        if (!actualString.contains(expectedSubstring)) {
            Assert.fail("String [" + actualString + "] doesn't contain substring ["
                    + expectedSubstring + "]");
        }
    }

    CronetEngine.Builder enableDiskCache(CronetEngine.Builder cronetEngineBuilder) {
        cronetEngineBuilder.setStoragePath(getTestStorage());
        cronetEngineBuilder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 1000 * 1024);
        return cronetEngineBuilder;
    }

    /**
     * Sets the {@link URLStreamHandlerFactory} from {@code cronetEngine}.  This should be called
     * during setUp() and is installed by {@link runTest()} as the default when Cronet is tested.
     */
    void setStreamHandlerFactory(CronetEngine cronetEngine) {
        mStreamHandlerFactory = cronetEngine.createURLStreamHandlerFactory();
    }

    URLStreamHandlerFactory getSteamHandlerFactory() {
        return mStreamHandlerFactory;
    }

    /**
     * Prepares the path for the test storage (http cache, QUIC server info).
     */
    static void prepareTestStorage(Context context) {
        File storage = new File(getTestStorageDirectory());
        if (storage.exists()) {
            Assert.assertTrue(recursiveDelete(storage));
        }
        ensureTestStorageExists();
    }
}
