// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.os.SystemClock;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.io.File;

public class TracingControllerAndroidTest extends ContentShellTestBase {

    private static final long TIMEOUT_MILLIS = scaleTimeout(30 * 1000);

    @MediumTest
    @Feature({"GPU"})
    public void testTraceFileCreation() throws Exception {
        ContentShellActivity activity = launchContentShellWithUrl("about:blank");
        waitForActiveShellToBeDoneLoading();

        final TracingControllerAndroid tracingController = new TracingControllerAndroid(activity);
        assertFalse(tracingController.isTracing());
        assertNull(tracingController.getOutputPath());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertTrue(tracingController.startTracing(true, "*", false));
            }
        });

        assertTrue(tracingController.isTracing());
        File file = new File(tracingController.getOutputPath());
        assertTrue(file.getName().startsWith("chrome-profile-results"));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tracingController.stopTracing();
            }
        });

        // The tracer stops asynchronously, because it needs to wait for native code to flush and
        // close the output file. Give it a little time.
        long startTime = SystemClock.uptimeMillis();
        while (tracingController.isTracing()) {
            if (SystemClock.uptimeMillis() > startTime + TIMEOUT_MILLIS) {
                fail("Timed out waiting for tracing to stop.");
            }
            Thread.sleep(1000);
        }

        // It says it stopped, so it should have written the output file.
        assertTrue(file.exists());
        assertTrue(file.delete());
        tracingController.destroy();
    }
}
