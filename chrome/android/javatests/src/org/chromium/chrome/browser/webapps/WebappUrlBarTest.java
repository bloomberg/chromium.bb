// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.components.security_state.ConnectionSecurityLevel;

/**
 * Tests whether the URL bar updates itself properly.
 */
public class WebappUrlBarTest extends WebappActivityTestBase {
    private static final String WEBAPP_URL = "http://originalwebsite.com";
    private WebappUrlBar mUrlBar;

    @Override
    protected Intent createIntent() {
        Intent intent = super.createIntent();
        intent.putExtra(ShortcutHelper.EXTRA_URL, WEBAPP_URL);
        return intent;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        startWebappActivity();
        mUrlBar = getActivity().getUrlBarForTests();
    }

    @UiThreadTest
    @MediumTest
    @Feature({"Webapps"})
    public void testUrlDisplay() {
        final String scheme = "https://";
        final String host = "lorem.com";
        final String path = "/stuff/and/things.html";
        final String url = scheme + host + path;
        final String urlExpectedWhenIconNotShown = scheme + host;
        final String urlExpectedWhenIconShown = host;
        final int[] securityLevels = {ConnectionSecurityLevel.NONE,
                ConnectionSecurityLevel.EV_SECURE, ConnectionSecurityLevel.SECURE,
                ConnectionSecurityLevel.SECURITY_WARNING,
                ConnectionSecurityLevel.SECURITY_POLICY_WARNING,
                ConnectionSecurityLevel.SECURITY_ERROR};

        for (int i : securityLevels) {
            // TODO(palmer): http://crbug.com/297249
            if (i == ConnectionSecurityLevel.SECURITY_POLICY_WARNING) continue;
            mUrlBar.update(url, i);

            int iconResource = mUrlBar.getCurrentIconResourceForTests();
            if (iconResource == 0) {
                assertEquals(
                        urlExpectedWhenIconNotShown, mUrlBar.getDisplayedUrlForTests().toString());
            } else {
                assertEquals(
                        urlExpectedWhenIconShown, mUrlBar.getDisplayedUrlForTests().toString());
            }
        }
    }
}
