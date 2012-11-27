// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewRenderView;

public class ChromiumTestShellUrlTest extends ChromiumTestShellTestBase {
    // URL used for base tests.
    private static final String URL = "data:text";

    @SmallTest
    @Feature({"Main"})
    public void testBaseStartup() throws Exception {
        ChromiumTestShellActivity activity = launchChromiumTestShellWithUrl(URL);
        waitForActiveShellToBeDoneLoading();

        // Make sure the activity was created as expected.
        assertNotNull(activity);
    }

    /**
     * Tests that creating an extra ContentViewRenderView does not cause an assert because we would
     * initialize the compositor twice http://crbug.com/162312
     */
    @SmallTest
    @Feature({"Main"})
    public void testCompositorInit() throws Exception {
        // Start the ChromiumTestShell, this loads the native library and create an instance of
        // ContentViewRenderView.
        final ChromiumTestShellActivity activity = launchChromiumTestShellWithUrl(URL);
        waitForActiveShellToBeDoneLoading();

        // Now create a new ContentViewRenderView, it should not assert.
        try {
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    ContentViewRenderView contentViewRenderView =
                            new ContentViewRenderView(getInstrumentation().getTargetContext());
                    contentViewRenderView.setCurrentContentView(activity.getActiveContentView());
                }
            });
        } catch (Throwable e) {
            fail("Could not create a ContentViewRenderView: " + e);
        }
    }
}