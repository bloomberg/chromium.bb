// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;

/**
 * Test suite for click listener validation in content detection.
 */
public class ClickListenerTest extends ContentDetectionTestBase {

    /*
    @MediumTest
    @Feature({"ContentDetection", "TabContents"})
    http://crbug.com/172786
    */
    @DisabledTest
    public void testClickContentOnLink() throws Throwable {
        startActivityWithTestUrl("content/content_detection/click_listeners.html");

        // Clicks on addresses in links should change the url.
        scrollAndTapNavigatingOut("linktest");
        assertTrue(isCurrentTestUrl("content/content_detection/empty.html"));
    }

    /*
    @MediumTest
    @Feature({"ContentDetection", "TabContents"})
    http://crbug.com/172786
    */
    @DisabledTest
    public void testClickContentOnJSListener1() throws Throwable {
        startActivityWithTestUrl("content/content_detection/click_listeners.html");

        // Clicks on addresses in elements listening to click events should be
        // processed normally without address detection.
        scrollAndTapNavigatingOut("clicktest1");
        assertTrue(isCurrentTestUrl("content/content_detection/empty.html"));
    }

    /*
    @MediumTest
    @Feature({"ContentDetection", "TabContents"})
    http://crbug.com/172786
    */
    @DisabledTest
    public void testClickContentOnJSListener2() throws Throwable {
        startActivityWithTestUrl("content/content_detection/click_listeners.html");

        // Same as previous test, but using addEventListener instead of onclick.
        scrollAndTapNavigatingOut("clicktest2");
        assertTrue(isCurrentTestUrl("content/content_detection/empty.html"));
    }
}
