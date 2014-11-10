// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

/**
 * Tests for {@link SessionDependencyFactoryNative}
 */
public class SessionDependencyFactoryNativeTest extends InstrumentationTestCase {
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        System.loadLibrary("devtools_bridge_natives_so");
    }

    @SmallTest
    public void testCreateInstance() {
        SessionDependencyFactoryNative instance = new SessionDependencyFactoryNative();
        instance.dispose();
    }
}
