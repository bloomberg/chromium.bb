// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.ssl.ConnectionSecurityLevel;
import org.chromium.content_public.common.ScreenOrientationValues;

/**
 * Tests whether the URL bar updates itself properly.
 */
public class WebappUrlBarTest extends WebappActivityTestBase {
    private static final String WEBAPP_URL = "http://originalwebsite.com";
    private WebappUrlBar mUrlBar;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        startWebappActivity();

        WebappInfo mockInfo = WebappInfo.create(WEBAPP_ID, WEBAPP_URL, null, null, null,
                ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING, false);
        getActivity().getWebappInfo().copy(mockInfo);
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
