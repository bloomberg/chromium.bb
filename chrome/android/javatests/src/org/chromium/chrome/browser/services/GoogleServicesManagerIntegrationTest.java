// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services;

import android.test.UiThreadTest;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;

/**
 * Google Services Manager tests
 */
public class GoogleServicesManagerIntegrationTest
        extends ChromeActivityTestCaseBase<ChromeActivity> {

    public GoogleServicesManagerIntegrationTest() {
        super(ChromeActivity.class);
    }

    /**
     * Test changing the state of auto login
     * @SmallTest
     * @Feature({"Sync", "Main"})
     */
    @DisabledTest(message = "crbug.com/413289")
    @UiThreadTest
    public void testSetAutologinState() {
        // TODO(acleung): Add back some sort of test for the GSM.
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
