// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.support.test.filters.SmallTest;

import org.junit.Assert;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.ui.base.PageTransition;

/**
 * Tests for UKM monitoring of incognito activity.
 */
@CommandLineFlags.Add({"force-enable-metrics-reporting"})
public class UkmIncognitoTest extends ChromeTabbedActivityTestBase {
    private static final String DEBUG_PAGE = "chrome://ukm";

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    public String getUkmState(Tab normalTab) throws Exception {
        loadUrlInTab(DEBUG_PAGE, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, normalTab);
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                normalTab.getContentViewCore().getWebContents(),
                "document.getElementById('state').textContent");
    }

    @SmallTest
    @RetryOnFailure
    public void testUkmIncognito() throws Exception {
        Tab normalTab = getActivity().getActivityTab();

        Assert.assertEquals("UKM State:", "\"True\"", getUkmState(normalTab));

        newIncognitoTabFromMenu();

        Assert.assertEquals("UKM State:", "\"False\"", getUkmState(normalTab));
    }
}
