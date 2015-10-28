// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.ssl.ConnectionSecurityLevel;
import org.chromium.content_public.common.ScreenOrientationValues;

/**
 * Tests the logic in top controls visibility delegate in WebappActivity.
 */
public class WebappVisibilityTest extends WebappActivityTestBase {
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        startWebappActivity();
    }

    @MediumTest
    @Feature({"Webapps"})
    public void testShouldShowTopControls() {
        final String webappUrl = "http://originalwebsite.com";
        WebappInfo mockInfo = WebappInfo.create(WEBAPP_ID, webappUrl, null,
                null, null, ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING, false);
        getActivity().getWebappInfo().copy(mockInfo);

        // Show top controls for out-of-domain URLs.
        assertTrue(getActivity().shouldShowTopControls(
                "http://notoriginalwebsite.com", ConnectionSecurityLevel.NONE));
        assertTrue(getActivity().shouldShowTopControls(
                "http://otherwebsite.com", ConnectionSecurityLevel.NONE));

        // Do not show top controls for subdomains and private registries that are secure.
        assertFalse(getActivity().shouldShowTopControls(
                "http://sub.originalwebsite.com", ConnectionSecurityLevel.NONE));
        assertFalse(getActivity().shouldShowTopControls(
                "http://thing.originalwebsite.com", ConnectionSecurityLevel.NONE));
        assertFalse(getActivity().shouldShowTopControls(webappUrl, ConnectionSecurityLevel.NONE));
        assertFalse(getActivity().shouldShowTopControls(
                webappUrl + "/things.html", ConnectionSecurityLevel.NONE));
        assertFalse(getActivity().shouldShowTopControls(
                webappUrl + "/stuff.html", ConnectionSecurityLevel.NONE));

        // Do not show top controls when URL is not available yet.
        assertFalse(getActivity().shouldShowTopControls("", ConnectionSecurityLevel.NONE));

        // Show top controls for non secure URLs.
        assertTrue(getActivity().shouldShowTopControls(
                "http://sub.originalwebsite.com", ConnectionSecurityLevel.SECURITY_WARNING));
        assertTrue(getActivity().shouldShowTopControls(
                "http://notoriginalwebsite.com", ConnectionSecurityLevel.SECURITY_ERROR));
        assertTrue(getActivity().shouldShowTopControls(
                "http://otherwebsite.com", ConnectionSecurityLevel.SECURITY_ERROR));
        assertTrue(getActivity().shouldShowTopControls(
                "http://thing.originalwebsite.com", ConnectionSecurityLevel.SECURITY_ERROR));
        assertTrue(getActivity().shouldShowTopControls(
                webappUrl, ConnectionSecurityLevel.SECURITY_WARNING));
        assertTrue(getActivity().shouldShowTopControls(
                webappUrl + "/things.html", ConnectionSecurityLevel.SECURITY_WARNING));
        assertTrue(getActivity().shouldShowTopControls(
                webappUrl + "/stuff.html", ConnectionSecurityLevel.SECURITY_WARNING));
    }
}
