// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell.rules;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.vr_shell.TestVrShellDelegate;
import org.chromium.chrome.browser.vr_shell.rules.VrActivityRestriction.SupportedActivity;
import org.chromium.chrome.browser.webapps.WebappActivityTestRule;

/**
 * VR extension of WebappActivityTestRule. Applies WebappActivityTestRule then opens
 * up a WebappActivity to a blank page.
 */
public class WebappActivityVrTestRule extends WebappActivityTestRule implements VrTestRule {
    @Override
    public Statement apply(final Statement base, Description desc) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                startWebappActivity();
                TestVrShellDelegate.createTestVrShellDelegate(getActivity());
                base.evaluate();
            }
        }, desc);
    }

    @Override
    public SupportedActivity getRestriction() {
        return SupportedActivity.WAA;
    }
}
