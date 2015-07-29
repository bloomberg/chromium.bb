// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test;

import org.chromium.base.test.util.HostDrivenTest;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;

/**
 * Dummy test suite for verifying the host-driven test framework.
 */
public class DummyTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    public DummyTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() {}

    @HostDrivenTest
    public void testPass() {}
}
