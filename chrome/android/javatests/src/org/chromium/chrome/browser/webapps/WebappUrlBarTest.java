// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content.browser.test.NativeLibraryTestRule;

/**
 * Tests whether the URL bar updates itself properly.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebappUrlBarTest {
    private Context mContext;

    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mActivityTestRule.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testUrlDisplay() {
        WebappUrlBar urlBar = new WebappUrlBar(mContext, null);

        final String scheme = "https://";
        final String host = "lorem.com";
        final String path = "/stuff/and/things.html";
        final String url = scheme + host + path;
        final String urlExpectedWhenIconNotShown = scheme + host;
        final String urlExpectedWhenIconShown = host;
        final int[] securityLevels = {ConnectionSecurityLevel.NONE,
                ConnectionSecurityLevel.EV_SECURE, ConnectionSecurityLevel.SECURE,
                ConnectionSecurityLevel.HTTP_SHOW_WARNING,
                ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT,
                ConnectionSecurityLevel.DANGEROUS};

        for (int i : securityLevels) {
            urlBar.update(url, i);

            int iconResource = urlBar.getCurrentIconResourceForTests();
            if (iconResource == 0) {
                Assert.assertEquals(
                        urlExpectedWhenIconNotShown, urlBar.getDisplayedUrlForTests().toString());
            } else {
                Assert.assertEquals(
                        urlExpectedWhenIconShown, urlBar.getDisplayedUrlForTests().toString());
            }
        }
    }
}
