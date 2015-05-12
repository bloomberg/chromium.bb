// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test;

import org.chromium.base.test.util.HostDrivenTest;
import org.chromium.chrome.shell.ChromeShellTestBase;

/**
 * Dummy test suite for verifying the host-driven test framework.
 */
public class DummyTest extends ChromeShellTestBase {
    @HostDrivenTest
    public void testPass() {}
}
