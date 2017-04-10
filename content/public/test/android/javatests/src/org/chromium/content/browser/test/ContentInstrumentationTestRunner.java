// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test;

import org.chromium.base.test.BaseChromiumInstrumentationTestRunner;
import org.chromium.base.test.BaseTestResult;

/**
 * An instrumentation test runner similar to BaseInstrumentationTestRunner but which also registers
 * content specific annotations.
 */
public class ContentInstrumentationTestRunner extends BaseChromiumInstrumentationTestRunner {
    @Override
    protected void addTestHooks(BaseTestResult result) {
        super.addTestHooks(result);
        result.addPreTestHook(new ChildProcessAllocatorSettingsHook());
    }
}
