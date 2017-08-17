// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.test.AndroidTestCase;

import org.chromium.base.Log;
import org.chromium.net.CronetTestCommon.CronetTestCommonCallback;
import org.chromium.net.CronetTestRule.CompareDefaultWithCronet;
import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.OnlyRunCronetHttpURLConnection;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.CronetTestRule.RequiresMinApi;
import org.chromium.net.impl.CronetEngineBase;
import org.chromium.net.impl.JavaCronetEngine;
import org.chromium.net.impl.UserAgent;

import java.lang.annotation.Annotation;
import java.lang.reflect.AnnotatedElement;
import java.net.URL;
import java.net.URLStreamHandlerFactory;

/**
 * Base test class for all CronetTest based tests.
 */
public class CronetTestBase extends AndroidTestCase implements CronetTestCommonCallback {
    private static final String TAG = CronetTestBase.class.getSimpleName();

    private final CronetTestCommon mTestCommon;

    public CronetTestBase() {
        mTestCommon = new CronetTestCommon(this);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestCommon.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        mTestCommon.tearDown();
        super.tearDown();
    }

    /**
     * Starts the CronetTest framework.
     */
    protected CronetTestFramework startCronetTestFramework() {
        return mTestCommon.startCronetTestFramework();
    }

    /**
     * Returns {@code true} when test is being run against system HttpURLConnection implementation.
     */
    protected boolean testingSystemHttpURLConnection() {
        return mTestCommon.testingSystemHttpURLConnection();
    }

    /**
     * Returns {@code true} when test is being run against the java implementation of CronetEngine.
     */
    protected boolean testingJavaImpl() {
        return mTestCommon.testingJavaImpl();
    }


    @Override
    protected void runTest() throws Throwable {
        mTestCommon.setTestingSystemHttpURLConnection(false);
        mTestCommon.setTestingJavaImpl(false);
        String packageName = getClass().getPackage().getName();

        // Find the API version required by the test.
        int requiredApiVersion = mTestCommon.getMaximumAvailableApiLevel();
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

        if (requiredApiVersion > mTestCommon.getMaximumAvailableApiLevel()) {
            Log.i(TAG,
                    getName() + " skipped because it requires API " + requiredApiVersion
                            + " but only API " + mTestCommon.getMaximumAvailableApiLevel()
                            + " is present.");
        } else if (packageName.equals("org.chromium.net.urlconnection")) {
            try {
                if (method.isAnnotationPresent(CompareDefaultWithCronet.class)) {
                    // Run with the default HttpURLConnection implementation first.
                    mTestCommon.setTestingSystemHttpURLConnection(true);
                    super.runTest();
                    // Use Cronet's implementation, and run the same test.
                    mTestCommon.setTestingSystemHttpURLConnection(false);
                    URL.setURLStreamHandlerFactory(mTestCommon.getSteamHandlerFactory());
                    super.runTest();
                } else if (method.isAnnotationPresent(OnlyRunCronetHttpURLConnection.class)) {
                    // Run only with Cronet's implementation.
                    URL.setURLStreamHandlerFactory(mTestCommon.getSteamHandlerFactory());
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
                    if (mTestCommon.getCronetTestFramework() != null) {
                        ExperimentalCronetEngine.Builder builder = createJavaEngineBuilder();
                        builder.setUserAgent(UserAgent.from(getContext()));
                        mTestCommon.getCronetTestFramework().mCronetEngine =
                                (CronetEngineBase) builder.build();
                        // Make sure that the instantiated engine is JavaCronetEngine.
                        assert mTestCommon.getCronetTestFramework().mCronetEngine.getClass()
                                == JavaCronetEngine.class;
                    }
                    mTestCommon.setTestingJavaImpl(true);
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
     * during setUp() and is installed by runTest() as the default when Cronet is tested.
     */
    public void setStreamHandlerFactory(CronetEngine cronetEngine) {
        mTestCommon.setStreamHandlerFactory(cronetEngine);
    }

    @Override
    public Context getContextForTestCommon() {
        return getContext();
    }

    /**
     * Prepares the path for the test storage (http cache, QUIC server info).
     */
    public static void prepareTestStorage(Context context) {
        CronetTestCommon.prepareTestStorage(context);
    }

    /**
     * Returns the path for the test storage (http cache, QUIC server info).
     * Also ensures it exists.
     */
    static String getTestStorage(Context context) {
        return CronetTestCommon.getTestStorage();
    }
}
