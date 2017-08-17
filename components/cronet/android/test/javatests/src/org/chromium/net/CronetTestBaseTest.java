// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.CronetTestRule.RequiresMinApi;
import org.chromium.net.impl.CronetUrlRequestContext;
import org.chromium.net.impl.JavaCronetEngine;

/**
 * Tests features of CronetTestBase.
 */
public class CronetTestBaseTest extends CronetTestBase {
    private CronetTestFramework mTestFramework;
    /**
     * For any test whose name contains "MustRun", it's enforced that the test must run and set
     * {@code mTestWasRun} to {@code true}.
     */
    private boolean mTestWasRun;
    /**
     * For {@link #testRunBothImplsMustRun}, use {@link #mTestNativeImplRun} to verify that the
     * test is run against the native implementation.
     */
    private boolean mTestNativeImplRun;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestFramework = startCronetTestFramework();
    }

    @Override
    protected void tearDown() throws Exception {
        if (getName().contains("MustRun") && !mTestWasRun) {
            fail(getName() + " should have run but didn't.");
        }
        super.tearDown();
    }

    @SmallTest
    @RequiresMinApi(999999999)
    @Feature({"Cronet"})
    public void testRequiresMinApiDisable() {
        fail("RequiresMinApi failed to disable.");
    }

    @SmallTest
    @RequiresMinApi(-999999999)
    @Feature({"Cronet"})
    public void testRequiresMinApiMustRun() {
        mTestWasRun = true;
    }

    @SmallTest
    @Feature({"Cronet"})
    public void testRunBothImplsMustRun() {
        if (testingJavaImpl()) {
            assertFalse(mTestWasRun);
            assertTrue(mTestNativeImplRun);
            mTestWasRun = true;
            assertEquals(mTestFramework.mCronetEngine.getClass(), JavaCronetEngine.class);
        } else {
            assertFalse(mTestWasRun);
            assertFalse(mTestNativeImplRun);
            mTestNativeImplRun = true;
            assertEquals(mTestFramework.mCronetEngine.getClass(), CronetUrlRequestContext.class);
        }
    }

    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testRunOnlyNativeMustRun() {
        assertFalse(testingJavaImpl());
        assertFalse(mTestWasRun);
        mTestWasRun = true;
        assertEquals(mTestFramework.mCronetEngine.getClass(), CronetUrlRequestContext.class);
    }
}