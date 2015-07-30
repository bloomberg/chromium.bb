// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

/**
 * This test exists so that ChromeShellTest.apk can compile. This can be deleted imminently when
 * ChromeShellTest is deleted.
 */
public class DummyTest extends InstrumentationTestCase {

    @SmallTest
    public void testNothing() {}

}
