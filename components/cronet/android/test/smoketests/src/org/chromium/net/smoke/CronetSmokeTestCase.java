// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import android.content.Context;
import android.test.AndroidTestCase;

import org.chromium.net.CronetEngine;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.UrlResponseInfo;

/**
 * Base test class. This class should not import any classes from the org.chromium.base package.
 */
public class CronetSmokeTestCase extends AndroidTestCase {
    /**
     * The key in the string resource file that specifies {@link TestSupport} that should
     * be instantiated.
     */
    private static final String SUPPORT_IMPL_RES_KEY = "TestSupportImplClass";

    protected ExperimentalCronetEngine.Builder mCronetEngineBuilder;
    protected CronetEngine mCronetEngine;
    protected TestSupport mTestSupport;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mCronetEngineBuilder = new ExperimentalCronetEngine.Builder(getContext());
        initTestSupport();
    }

    @Override
    protected void tearDown() throws Exception {
        if (mCronetEngine != null) {
            mCronetEngine.shutdown();
        }
        super.tearDown();
    }

    protected void initCronetEngine() {
        mCronetEngine = mCronetEngineBuilder.build();
    }

    static void assertSuccessfulNonEmptyResponse(SmokeTestRequestCallback callback, String url) {
        // Check the request state
        if (callback.getFinalState() == SmokeTestRequestCallback.State.Failed) {
            throw new RuntimeException(
                    "The request failed with an error", callback.getFailureError());
        }
        assertEquals(SmokeTestRequestCallback.State.Succeeded, callback.getFinalState());

        // Check the response info
        UrlResponseInfo responseInfo = callback.getResponseInfo();
        assertNotNull(responseInfo);
        assertFalse(responseInfo.wasCached());
        assertEquals(url, responseInfo.getUrl());
        assertEquals(url, responseInfo.getUrlChain().get(responseInfo.getUrlChain().size() - 1));
        assertEquals(200, responseInfo.getHttpStatusCode());
        assertTrue(responseInfo.toString().length() > 0);
    }

    static void assertJavaEngine(CronetEngine engine) {
        assertNotNull(engine);
        assertEquals("org.chromium.net.impl.JavaCronetEngine", engine.getClass().getName());
    }

    static void assertNativeEngine(CronetEngine engine) {
        assertNotNull(engine);
        assertEquals("org.chromium.net.impl.CronetUrlRequestContext", engine.getClass().getName());
    }

    /**
     * Instantiates a concrete implementation of {@link TestSupport} interface.
     * The name of the implementation class is determined dynamically by reading
     * the value of |TestSupportImplClass| from the Android string resource file.
     *
     * @throws Exception if the class cannot be instantiated.
     */
    private void initTestSupport() throws Exception {
        Context ctx = getContext();
        String packageName = ctx.getPackageName();
        int resId = ctx.getResources().getIdentifier(SUPPORT_IMPL_RES_KEY, "string", packageName);
        String className = ctx.getResources().getString(resId);
        Class<? extends TestSupport> cl = Class.forName(className).asSubclass(TestSupport.class);
        mTestSupport = cl.newInstance();
    }
}
