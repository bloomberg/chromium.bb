// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.StrictMode;
import android.test.AndroidTestCase;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.net.impl.CronetEngineBase;
import org.chromium.net.impl.JavaCronetEngine;
import org.chromium.net.impl.JavaCronetProvider;
import org.chromium.net.impl.UserAgent;

import java.io.File;
import java.lang.annotation.Annotation;
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

    private static final String TAG = CronetTestBase.class.getSimpleName();

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
    private StrictMode.VmPolicy mOldVmPolicy;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        System.loadLibrary("cronet_tests");
        ContextUtils.initApplicationContext(getContext().getApplicationContext());
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        prepareTestStorage(getContext());
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

    @SuppressFBWarnings("DM_GC") // Used to trigger strictmode detecting leaked closeables
    @Override
    protected void tearDown() throws Exception {
        try {
            // Run GC and finalizers a few times to pick up leaked closeables
            for (int i = 0; i < 10; i++) {
                System.gc();
                System.runFinalization();
            }
            System.gc();
            System.runFinalization();
            super.tearDown();
        } finally {
            StrictMode.setVmPolicy(mOldVmPolicy);
        }
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

    private int getMaximumAvailableApiLevel() {
        // Prior to M59 the ApiVersion.getMaximumAvailableApiLevel API didn't exist
        if (ApiVersion.getCronetVersion().compareTo("59") < 0) {
            return 3;
        }
        return ApiVersion.getMaximumAvailableApiLevel();
    }

    @Override
    protected void runTest() throws Throwable {
        mTestingSystemHttpURLConnection = false;
        mTestingJavaImpl = false;
        String packageName = getClass().getPackage().getName();

        // Find the API version required by the test.
        int requiredApiVersion = getMaximumAvailableApiLevel();
        for (Annotation a : getClass().getAnnotations()) {
            if (a instanceof RequiresMinApi) {
                requiredApiVersion = ((RequiresMinApi) a).value();
            }
        }
        AnnotatedElement method = getClass().getMethod(getName(), (Class[]) null);
        for (Annotation a : method.getAnnotations()) {
            if (a instanceof RequiresMinApi) {
                // Method scoped requirements take precedence over class scoped
                // requirements.
                requiredApiVersion = ((RequiresMinApi) a).value();
            }
        }

        if (requiredApiVersion > getMaximumAvailableApiLevel()) {
            Log.i(TAG,
                    getName() + " skipped because it requires API " + requiredApiVersion
                            + " but only API " + getMaximumAvailableApiLevel() + " is present.");
        } else if (packageName.equals("org.chromium.net.urlconnection")) {
            try {
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

    /**
     * Annotation for test methods in org.chromium.net.urlconnection pacakage that runs them
     * against both Cronet's HttpURLConnection implementation, and against the system's
     * HttpURLConnection implementation.
     */
    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface CompareDefaultWithCronet {
    }

    /**
     * Annotation for test methods in org.chromium.net.urlconnection pacakage that runs them
     * only against Cronet's HttpURLConnection implementation, and not against the system's
     * HttpURLConnection implementation.
     */
    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface OnlyRunCronetHttpURLConnection {
    }

    /**
     * Annotation for test methods in org.chromium.net package that disables rerunning the test
     * against the Java-only implementation. When this annotation is present the test is only run
     * against the native implementation.
     */
    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface OnlyRunNativeCronet {}

    /**
     * Annotation allowing classes or individual tests to be skipped based on the version of the
     * Cronet API present. Takes the minimum API version upon which the test should be run.
     * For example if a test should only be run with API version 2 or greater:
     *   @RequiresMinApi(2)
     *   public void testFoo() {}
     */
    @Target({ElementType.TYPE, ElementType.METHOD})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface RequiresMinApi {
        int value();
    }

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
