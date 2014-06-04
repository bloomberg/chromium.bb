// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo;

import android.content.Context;
import android.test.InstrumentationTestCase;

import org.chromium.base.JNINamespace;
import org.chromium.base.library_loader.LibraryLoader;

/**
 * Base class to test mojo. Setup the environment.
 */
@JNINamespace("mojo::android")
public class MojoTestCase extends InstrumentationTestCase {

    private long mTestEnvironmentPointer;

    /**
     * @see junit.framework.TestCase#setUp()
     */
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        LibraryLoader.ensureInitialized();
        nativeInitApplicationContext(getInstrumentation().getTargetContext());
        mTestEnvironmentPointer = nativeSetupTestEnvironment();
    }

    /**
     * @see android.test.InstrumentationTestCase#tearDown()
     */
    @Override
    protected void tearDown() throws Exception {
        nativeTearDownTestEnvironment(mTestEnvironmentPointer);
        super.tearDown();
    }

    private native void nativeInitApplicationContext(Context context);

    private native long nativeSetupTestEnvironment();

    private native void nativeTearDownTestEnvironment(long testEnvironment);

    protected native void nativeRunLoop(long timeoutMS);

}
