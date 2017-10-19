// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell.rules;

import android.support.test.InstrumentationRegistry;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.vr_shell.TestVrShellDelegate;
import org.chromium.chrome.browser.vr_shell.rules.VrActivityRestriction.SupportedActivity;

/**
 * VR extension of CustomTabActivityTestRule. Applies CustomTabActivityTestRule then
 * opens up a CustomTabActivity to a blank page.
 */
public class CustomTabActivityVrTestRule extends CustomTabActivityTestRule implements VrTestRule {
    @Override
    public Statement apply(final Statement base, Description desc) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), "about:blank"));
                TestVrShellDelegate.createTestVrShellDelegate(getActivity());
                base.evaluate();
            }
        }, desc);
    }

    @Override
    public SupportedActivity getRestriction() {
        return SupportedActivity.CCT;
    }
}
