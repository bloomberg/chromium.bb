// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwDebug;
import org.chromium.base.test.util.Feature;

import java.io.File;

/**
 * A test suite for AwDebug class.
 */
public class AwDebugTest extends AwTestBase {
    @SmallTest
    @Feature({"AndroidWebView", "Debug"})
    public void testDump() throws Throwable {
        File f = File.createTempFile("dump", ".dmp");
        try {
            assertTrue(AwDebug.dumpWithoutCrashing(f));
            assertTrue(f.canRead());
            assertTrue(f.length() != 0);
        } finally {
            assertTrue(f.delete());
        }
    }
}
