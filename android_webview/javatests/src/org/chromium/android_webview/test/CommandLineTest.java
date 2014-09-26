// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.base.CommandLine;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.Feature;

/**
 * Test suite for setting by the command line.
 */
public class CommandLineTest extends AwTestBase {
    @Override
    protected boolean needsBrowserProcessStarted() {
        return false;
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSetupCommandLine() throws Exception {
        // The commandline starts off in Java:
        CommandLine cl = CommandLine.getInstance();
        assertFalse(cl.isNativeImplementation());

        // We can add a switch.
        assertFalse(cl.hasSwitch("magic-switch"));
        cl.appendSwitchWithValue("magic-switch", "magic");
        assertTrue(cl.hasSwitch("magic-switch"));
        assertEquals("magic", cl.getSwitchValue("magic-switch"));

        // Setup Chrome.
        AwBrowserProcess.loadLibrary();
        LibraryLoader.switchCommandLineForWebView();

        // Now we should have switched to a native backed command line:
        cl = CommandLine.getInstance();
        assertTrue(cl.isNativeImplementation());

        // Our first switch is still there.
        assertTrue(cl.hasSwitch("magic-switch"));
        assertEquals("magic", cl.getSwitchValue("magic-switch"));

        // And we can add another one.
        assertFalse(cl.hasSwitch("more-magic-switch"));
        cl.appendSwitchWithValue("more-magic-switch", "more-magic");
        assertTrue(cl.hasSwitch("more-magic-switch"));
        assertEquals("more-magic", cl.getSwitchValue("more-magic-switch"));
    }
}
