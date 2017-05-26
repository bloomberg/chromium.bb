// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.test.AndroidTestCase;

import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.net.impl.CronetEngineBase;
import org.chromium.net.impl.JavaCronetEngine;
import org.chromium.net.impl.JavaCronetProvider;
import org.chromium.net.impl.UserAgent;

import java.io.File;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.reflect.AnnotatedElement;
import java.net.URL;
import java.net.URLStreamHandlerFactory;

/**
 * Base test class for all CronetTest based tests.
 */
public class CronetTestBase extends AndroidTestCase {
    /**
     * Creates and holds pointer to CronetEngine.
     */
    public static class CronetTestFramework {
        public CronetEngineBase mCronetEngine;

        public CronetTestFramework(Context context) {
            mCronetEngine = (CronetEngineBase) new ExperimentalCronetEngine.Builder(context)
                                    .enableQuic(true)
                                    .build();
            // Start collecting metrics.
            mCronetEngine.getGlobalMetricsDeltas();
        }
    }

    /**
     * Name of the file that contains the test server certificate in PEM format.
     */
    static final String SERVER_CERT_PEM = "quic_test.example.com.crt";

    /**
     * Name of the file that contains the test server private key in PKCS8 PEM format.
     */
    static final String SERVER_KEY_PKCS8_PEM = "quic_test.example.com.key.pkcs8.pem";

    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "cronet_test";
    private static final String LOOPBACK_ADDRESS = "127.0.0.1";

    private CronetTestFramework mCronetTestFramework;
    private URLStreamHandlerFactory mStreamHandlerFactory;

    // {@code true} when test is being run against system HttpURLConnection implementation.
    private boolean mTestingSystemHttpURLConnection;
    private boolean mTestingJavaImpl = false;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        System.loadLibrary("cronet_tests");
        ContextUtils.initApplicationContext(getContext().getApplicationContext());
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        prepareTestStorage(getContext());
    }

    /**
     * Starts the CronetTest framework.
     */
    protected CronetTestFramework startCronetTestFramework() {
        mCronetTestFramework = new CronetTestFramework(getContext());
        return mCronetTestFramework;
    }

    /**
     * Returns {@code true} when test is being run against system HttpURLConnection implementation.
     */
    protected boolean testingSystemHttpURLConnection() {
        return mTestingSystemHttpURLConnection;
    }

    /**
     * Returns {@code true} when test is being run against the java implementation of CronetEngine.
     */
    protected boolean testingJavaImpl() {
        return mTestingJavaImpl;
    }

    @Override
    protected void runTest() throws Throwable {
        mTestingSystemHttpURLConnection = false;
        mTestingJavaImpl = false;
        String packageName = getClass().getPackage().getName();
        if (packageName.equals("org.chromium.net.urlconnection")) {
            try {
                AnnotatedElement method = getClass().getMethod(getName(), (Class[]) null);
                if (method.isAnnotationPresent(CompareDefaultWithCronet.class)) {
                    // Run with the default HttpURLConnection implementation first.
                    mTestingSystemHttpURLConnection = true;
                    super.runTest();
                    // Use Cronet's implementation, and run the same test.
                    mTestingSystemHttpURLConnection = false;
                    URL.setURLStreamHandlerFactory(mStreamHandlerFactory);
                    super.runTest();
                } else if (method.isAnnotationPresent(OnlyRunCronetHttpURLConnection.class)) {
                    // Run only with Cronet's implementation.
                    URL.setURLStreamHandlerFactory(mStreamHandlerFactory);
                    super.runTest();
                } else {
                    // For all other tests.
                    super.runTest();
                }
            } catch (Throwable e) {
                throw new Throwable("CronetTestBase#runTest failed.", e);
            }
        } else if (packageName.equals("org.chromium.net")) {
            try {
                AnnotatedElement method = getClass().getMethod(getName(), (Class[]) null);
                super.runTest();
                if (!method.isAnnotationPresent(OnlyRunNativeCronet.class)) {
                    if (mCronetTestFramework != null) {
                        ExperimentalCronetEngine.Builder builder = createJavaEngineBuilder();
                        builder.setUserAgent(UserAgent.from(getContext()));
                        mCronetTestFramework.mCronetEngine = (CronetEngineBase) builder.build();
                        // Make sure that the instantiated engine is JavaCronetEngine.
                        assert mCronetTestFramework.mCronetEngine.getClass()
                                == JavaCronetEngine.class;
                    }
                    mTestingJavaImpl = true;
                    super.runTest();
                }
            } catch (Throwable e) {
                throw new Throwable("CronetTestBase#runTest failed.", e);
            }
        } else {
            super.runTest();
        }
    }

    /**
     * Creates and returns {@link ExperimentalCronetEngine.Builder} that creates
     * Java (platform) based {@link CronetEngine.Builder}.
     *
     * @return the {@code CronetEngine.Builder} that builds Java-based {@code Cronet engine}.
     */
    ExperimentalCronetEngine.Builder createJavaEngineBuilder() {
        return (ExperimentalCronetEngine.Builder) new JavaCronetProvider(mContext).createBuilder();
    }

    public void assertResponseEquals(UrlResponseInfo expected, UrlResponseInfo actual) {
        assertEquals(expected.getAllHeaders(), actual.getAllHeaders());
        assertEquals(expected.getAllHeadersAsList(), actual.getAllHeadersAsList());
        assertEquals(expected.getHttpStatusCode(), actual.getHttpStatusCode());
        assertEquals(expected.getHttpStatusText(), actual.getHttpStatusText());
        assertEquals(expected.getUrlChain(), actual.getUrlChain());
        assertEquals(expected.getUrl(), actual.getUrl());
        // Transferred bytes and proxy server are not supported in pure java
        if (!testingJavaImpl()) {
            assertEquals(expected.getReceivedByteCount(), actual.getReceivedByteCount());
            assertEquals(expected.getProxyServer(), actual.getProxyServer());
            // This is a place where behavior intentionally differs between native and java
            assertEquals(expected.getNegotiatedProtocol(), actual.getNegotiatedProtocol());
        }
    }

    public static void assertContains(String expectedSubstring, String actualString) {
        assertNotNull(actualString);
        if (!actualString.contains(expectedSubstring)) {
            fail("String [" + actualString + "] doesn't contain substring [" + expectedSubstring
                    + "]");
        }
    }

    public CronetEngine.Builder enableDiskCache(CronetEngine.Builder cronetEngineBuilder) {
        cronetEngineBuilder.setStoragePath(getTestStorage(getContext()));
        cronetEngineBuilder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 1000 * 1024);
        return cronetEngineBuilder;
    }

    /**
     * Sets the {@link URLStreamHandlerFactory} from {@code cronetEngine}.  This should be called
     * during setUp() and is installed by {@link runTest()} as the default when Cronet is tested.
     */
    public void setStreamHandlerFactory(CronetEngine cronetEngine) {
        mStreamHandlerFactory = cronetEngine.createURLStreamHandlerFactory();
    }

    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface CompareDefaultWithCronet {
    }

    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface OnlyRunCronetHttpURLConnection {
    }

    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface OnlyRunNativeCronet {}

    /**
     * Prepares the path for the test storage (http cache, QUIC server info).
     */
    public static void prepareTestStorage(Context context) {
        File storage = new File(getTestStorageDirectory());
        if (storage.exists()) {
            assertTrue(recursiveDelete(storage));
        }
        ensureTestStorageExists();
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
            assertTrue(storage.mkdir());
        }
    }

    /**
     * Returns the path for the test storage (http cache, QUIC server info).
     * Also ensures it exists.
     */
    static String getTestStorage(Context context) {
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
}
