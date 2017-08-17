// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.support.test.InstrumentationRegistry;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.Log;
import org.chromium.net.CronetTestCommon.CronetTestCommonCallback;
import org.chromium.net.impl.CronetEngineBase;
import org.chromium.net.impl.JavaCronetEngine;
import org.chromium.net.impl.UserAgent;

import java.lang.annotation.Annotation;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.net.URL;
import java.net.URLStreamHandlerFactory;

/**
 * Custom TestRule for Cronet instrumentation tests.
 *
 * TODO(yolandyan): refactor this to three TestRules, one for setUp and tearDown,
 * one for tests under org.chromium.net.urlconnection, one for test under
 * org.chromium.net
 */
public class CronetTestRule implements CronetTestCommonCallback, TestRule {
    /**
     * Name of the file that contains the test server certificate in PEM format.
     */
    public static final String SERVER_CERT_PEM = "quic_test.example.com.crt";

    /**
     * Name of the file that contains the test server private key in PKCS8 PEM format.
     */
    public static final String SERVER_KEY_PKCS8_PEM = "quic_test.example.com.key.pkcs8.pem";

    private static final String TAG = CronetTestRule.class.getSimpleName();

    private final CronetTestCommon mTestCommon;

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

    public CronetTestRule() {
        mTestCommon = new CronetTestCommon(this);
    }

    @Override
    public Statement apply(final Statement base, final Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUp();
                runBase(base, desc);
                tearDown();
            }
        };
    }

    /**
     * Returns {@code true} when test is being run against system HttpURLConnection implementation.
     */
    public boolean testingSystemHttpURLConnection() {
        return mTestCommon.testingSystemHttpURLConnection();
    }

    /**
     * Returns {@code true} when test is being run against the java implementation of CronetEngine.
     */
    public boolean testingJavaImpl() {
        return mTestCommon.testingJavaImpl();
    }

    // TODO(yolandyan): refactor this using parameterize framework
    private void runBase(Statement base, Description desc) throws Throwable {
        mTestCommon.setTestingSystemHttpURLConnection(false);
        mTestCommon.setTestingJavaImpl(false);
        String packageName = desc.getTestClass().getPackage().getName();

        // Find the API version required by the test.
        int requiredApiVersion = mTestCommon.getMaximumAvailableApiLevel();
        for (Annotation a : desc.getAnnotations()) {
            if (a instanceof RequiresMinApi) {
                requiredApiVersion = ((RequiresMinApi) a).value();
            }
        }
        for (Annotation a : desc.getAnnotations()) {
            if (a instanceof RequiresMinApi) {
                // Method scoped requirements take precedence over class scoped
                // requirements.
                requiredApiVersion = ((RequiresMinApi) a).value();
            }
        }

        if (requiredApiVersion > mTestCommon.getMaximumAvailableApiLevel()) {
            Log.i(TAG,
                    desc.getMethodName() + " skipped because it requires API " + requiredApiVersion
                            + " but only API " + mTestCommon.getMaximumAvailableApiLevel()
                            + " is present.");
        } else if (packageName.equals("org.chromium.net.urlconnection")) {
            try {
                if (desc.getAnnotation(CompareDefaultWithCronet.class) != null) {
                    // Run with the default HttpURLConnection implementation first.
                    mTestCommon.setTestingSystemHttpURLConnection(true);
                    base.evaluate();
                    // Use Cronet's implementation, and run the same test.
                    mTestCommon.setTestingSystemHttpURLConnection(false);
                    URL.setURLStreamHandlerFactory(mTestCommon.getSteamHandlerFactory());
                    base.evaluate();
                } else if (desc.getAnnotation(OnlyRunCronetHttpURLConnection.class) != null) {
                    // Run only with Cronet's implementation.
                    URL.setURLStreamHandlerFactory(mTestCommon.getSteamHandlerFactory());
                    base.evaluate();
                } else {
                    // For all other tests.
                    base.evaluate();
                }
            } catch (Throwable e) {
                throw new Throwable("Cronet Test failed.", e);
            }
        } else if (packageName.equals("org.chromium.net")) {
            try {
                base.evaluate();
                if (desc.getAnnotation(OnlyRunNativeCronet.class) == null) {
                    if (mTestCommon.getCronetTestFramework() != null) {
                        ExperimentalCronetEngine.Builder builder = createJavaEngineBuilder();
                        builder.setUserAgent(
                                UserAgent.from(InstrumentationRegistry.getTargetContext()));
                        mTestCommon.getCronetTestFramework().mCronetEngine =
                                (CronetEngineBase) builder.build();
                        // Make sure that the instantiated engine is JavaCronetEngine.
                        assert mTestCommon.getCronetTestFramework().mCronetEngine.getClass()
                                == JavaCronetEngine.class;
                    }
                    mTestCommon.setTestingJavaImpl(true);
                    base.evaluate();
                }
            } catch (Throwable e) {
                throw new Throwable("CronetTestBase#runTest failed.", e);
            }
        } else {
            base.evaluate();
        }
    }

    void setUp() throws Exception {
        mTestCommon.setUp();
    }

    void tearDown() throws Exception {
        mTestCommon.tearDown();
    }

    public CronetTestFramework startCronetTestFramework() {
        return mTestCommon.startCronetTestFramework();
    }

    /**
     * Creates and returns {@link ExperimentalCronetEngine.Builder} that creates
     * Java (platform) based {@link CronetEngine.Builder}.
     *
     * @return the {@code CronetEngine.Builder} that builds Java-based {@code Cronet engine}.
     */
    public ExperimentalCronetEngine.Builder createJavaEngineBuilder() {
        return mTestCommon.createJavaEngineBuilder();
    }

    public void assertResponseEquals(UrlResponseInfo expected, UrlResponseInfo actual) {
        mTestCommon.assertResponseEquals(expected, actual);
    }

    public static void assertContains(String expectedSubstring, String actualString) {
        CronetTestCommon.assertContains(expectedSubstring, actualString);
    }

    public CronetEngine.Builder enableDiskCache(CronetEngine.Builder cronetEngineBuilder) {
        return mTestCommon.enableDiskCache(cronetEngineBuilder);
    }

    /**
     * Sets the {@link URLStreamHandlerFactory} from {@code cronetEngine}.  This should be called
     * during setUp() and is installed by {@link runTest()} as the default when Cronet is tested.
     */
    public void setStreamHandlerFactory(CronetEngine cronetEngine) {
        mTestCommon.setStreamHandlerFactory(cronetEngine);
    }

    /**
     * Annotation for test methods in org.chromium.net.urlconnection pacakage that runs them
     * against both Cronet's HttpURLConnection implementation, and against the system's
     * HttpURLConnection implementation.
     */
    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface CompareDefaultWithCronet {}

    /**
     * Annotation for test methods in org.chromium.net.urlconnection pacakage that runs them
     * only against Cronet's HttpURLConnection implementation, and not against the system's
     * HttpURLConnection implementation.
     */
    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface OnlyRunCronetHttpURLConnection {}

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
        CronetTestCommon.prepareTestStorage(context);
    }

    @Override
    public Context getContextForTestCommon() {
        return InstrumentationRegistry.getTargetContext();
    }

    /**
     * Returns the path for the test storage (http cache, QUIC server info).
     * Also ensures it exists.
     */
    public static String getTestStorage(Context context) {
        return CronetTestCommon.getTestStorage();
    }
}
