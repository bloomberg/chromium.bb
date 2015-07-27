// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;

/**
 * Basic shared functionality for partner customization integration tests.
 */
public abstract class BasePartnerBrowserCustomizationIntegrationTest extends
        ChromeActivityTestCaseBase<ChromeActivity> {

    public BasePartnerBrowserCustomizationIntegrationTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                TestPartnerBrowserCustomizationsProvider.class.getName());
        super.setUp();
    }
}
