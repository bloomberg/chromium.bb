// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.support.test.InstrumentationRegistry;

import org.junit.runners.model.InitializationError;

import org.chromium.base.CollectionUtil;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.BaseTestResult.PreTestHook;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.SkipCheck;
import org.chromium.chrome.test.ChromeInstrumentationTestRunner.ChromeDisableIfSkipCheck;
import org.chromium.chrome.test.ChromeInstrumentationTestRunner.ChromeRestrictionSkipCheck;
import org.chromium.content.browser.test.ChildProcessAllocatorSettingsHook;
import org.chromium.policy.test.annotations.Policies;

import java.util.List;

/**
 * A custom runner for //chrome JUnit4 tests.
 */
public class ChromeJUnit4ClassRunner extends BaseJUnit4ClassRunner {
    /**
     * Create a ChromeJUnit4ClassRunner to run {@code klass} and initialize values
     *
     * @throws InitializationError if the test class malformed
     */
    public ChromeJUnit4ClassRunner(final Class<?> klass) throws InitializationError {
        super(klass, defaultSkipChecks(), defaultPreTestHooks());
    }

    private static List<SkipCheck> defaultSkipChecks() {
        return CollectionUtil.newArrayList(
                new ChromeRestrictionSkipCheck(InstrumentationRegistry.getTargetContext()),
                new ChromeDisableIfSkipCheck(InstrumentationRegistry.getTargetContext()));
    }

    private static List<PreTestHook> defaultPreTestHooks() {
        return CollectionUtil.newArrayList(CommandLineFlags.getRegistrationHook(),
                new ChildProcessAllocatorSettingsHook(), Policies.getRegistrationHook());
    }

}
