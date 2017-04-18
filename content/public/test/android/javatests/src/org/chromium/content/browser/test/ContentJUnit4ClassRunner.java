// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test;

import org.junit.runners.model.InitializationError;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.BaseTestResult.PreTestHook;
import org.chromium.base.test.util.CommandLineFlags;

import java.util.Arrays;
import java.util.List;

/**
 * A custom runner for //content JUnit4 tests.
 */
public class ContentJUnit4ClassRunner extends BaseJUnit4ClassRunner {
    /**
     * Create a ContentJUnit4ClassRunner to run {@code klass} and initialize values
     *
     * @throws InitializationError if the test class malformed
     */
    public ContentJUnit4ClassRunner(final Class<?> klass) throws InitializationError {
        super(klass, null, defaultPreTestHooks());
    }

    /**
     * Change this static function to add default {@code PreTestHook}s.
     */
    private static List<PreTestHook> defaultPreTestHooks() {
        return Arrays.asList(new PreTestHook[] {
            CommandLineFlags.getRegistrationHook(),
            new ChildProcessAllocatorSettingsHook()});
    }
}
