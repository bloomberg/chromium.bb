// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.test.AndroidTestCase;

import org.chromium.base.PathUtils;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.reflect.Method;
import java.net.URL;

/**
 * Base test class for all CronetTest based tests.
 */
public class CronetTestBase extends AndroidTestCase {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "cronet_test";

    private CronetTestFramework mCronetTestFramework;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX, getContext());
    }

    /**
     * Starts the CronetTest framework.
     */
    protected CronetTestFramework startCronetTestFramework() {
        return startCronetTestFrameworkWithUrlAndCronetEngineBuilder(null, null);
    }

    /**
     * Starts the CronetTest framework and loads the given URL. The URL can be
     * null.
     */
    protected CronetTestFramework startCronetTestFrameworkWithUrl(String url) {
        return startCronetTestFrameworkWithUrlAndCronetEngineBuilder(url, null);
    }

    /**
     * Starts the CronetTest framework using the provided CronetEngine.Builder
     * and loads the given URL. The URL can be null.
     */
    protected CronetTestFramework startCronetTestFrameworkWithUrlAndCronetEngineBuilder(
            String url, CronetEngine.Builder builder) {
        mCronetTestFramework = new CronetTestFramework(url, null, getContext(), builder);
        return mCronetTestFramework;
    }

    /**
     * Starts the CronetTest framework appending the provided command line
     * arguments and loads the given URL. The URL can be null.
     */
    protected CronetTestFramework startCronetTestFrameworkWithUrlAndCommandLineArgs(
            String url, String[] commandLineArgs) {
        mCronetTestFramework = new CronetTestFramework(url, commandLineArgs, getContext(), null);
        return mCronetTestFramework;
    }

    // Helper method to tell the framework to skip factory init during construction.
    protected CronetTestFramework startCronetTestFrameworkAndSkipFactoryInit() {
        String[] commandLineArgs = {
                CronetTestFramework.LIBRARY_INIT_KEY, CronetTestFramework.LIBRARY_INIT_SKIP};
        mCronetTestFramework =
                startCronetTestFrameworkWithUrlAndCommandLineArgs(null, commandLineArgs);
        return mCronetTestFramework;
    }

    @Override
    protected void runTest() throws Throwable {
        if (!getClass().getPackage().getName().equals(
                "org.chromium.net.urlconnection")) {
            super.runTest();
            return;
        }
        try {
            Method method = getClass().getMethod(getName(), (Class[]) null);
            if (method.isAnnotationPresent(CompareDefaultWithCronet.class)) {
                // Run with the default HttpURLConnection implementation first.
                super.runTest();
                // Use Cronet's implementation, and run the same test.
                URL.setURLStreamHandlerFactory(mCronetTestFramework.mStreamHandlerFactory);
                super.runTest();
            } else if (method.isAnnotationPresent(
                    OnlyRunCronetHttpURLConnection.class)) {
                // Run only with Cronet's implementation.
                URL.setURLStreamHandlerFactory(mCronetTestFramework.mStreamHandlerFactory);
                super.runTest();
            } else {
                // For all other tests.
                super.runTest();
            }
        } catch (Throwable e) {
            throw new Throwable("CronetTestBase#runTest failed.", e);
        }
    }

    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface CompareDefaultWithCronet {
    }

    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface OnlyRunCronetHttpURLConnection {
    }

}
