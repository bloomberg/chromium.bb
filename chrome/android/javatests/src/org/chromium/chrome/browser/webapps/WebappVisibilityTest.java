// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/**
 * Tests for {@link WebappDelegateFactory}.
 */
public class WebappVisibilityTest extends NativeLibraryTestBase {
    private static final String WEBAPP_URL = "http://originalwebsite.com";

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryNoBrowserProcess();
    }

    @MediumTest
    @Feature({"Webapps"})
    public void testShouldShowBrowserControls() {
        // Show browser controls for out-of-domain URLs.
        assertTrue(shouldShowBrowserControls(
                WEBAPP_URL, "http://notoriginalwebsite.com", ConnectionSecurityLevel.NONE));
        assertTrue(shouldShowBrowserControls(
                WEBAPP_URL, "http://otherwebsite.com", ConnectionSecurityLevel.NONE));

        // Do not show browser controls for subdomains and private registries that are secure.
        assertFalse(shouldShowBrowserControls(
                WEBAPP_URL, "http://sub.originalwebsite.com", ConnectionSecurityLevel.NONE));
        assertFalse(shouldShowBrowserControls(
                WEBAPP_URL, "http://thing.originalwebsite.com", ConnectionSecurityLevel.NONE));
        assertFalse(
                shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL, ConnectionSecurityLevel.NONE));
        assertFalse(shouldShowBrowserControls(
                WEBAPP_URL, WEBAPP_URL + "/things.html", ConnectionSecurityLevel.NONE));
        assertFalse(shouldShowBrowserControls(
                WEBAPP_URL, WEBAPP_URL + "/stuff.html", ConnectionSecurityLevel.NONE));

        // Do not show browser controls when URL is not available yet.
        assertFalse(shouldShowBrowserControls(WEBAPP_URL, "", ConnectionSecurityLevel.NONE));

        // Show browser controls for non secure URLs.
        assertTrue(shouldShowBrowserControls(WEBAPP_URL, "http://sub.originalwebsite.com",
                ConnectionSecurityLevel.SECURITY_WARNING));
        assertTrue(shouldShowBrowserControls(
                WEBAPP_URL, "http://notoriginalwebsite.com", ConnectionSecurityLevel.DANGEROUS));
        assertTrue(shouldShowBrowserControls(
                WEBAPP_URL, "http://otherwebsite.com", ConnectionSecurityLevel.DANGEROUS));
        assertTrue(shouldShowBrowserControls(
                WEBAPP_URL, "http://thing.originalwebsite.com", ConnectionSecurityLevel.DANGEROUS));
        assertTrue(shouldShowBrowserControls(
                WEBAPP_URL, WEBAPP_URL, ConnectionSecurityLevel.SECURITY_WARNING));
        assertTrue(shouldShowBrowserControls(
                WEBAPP_URL, WEBAPP_URL + "/things.html", ConnectionSecurityLevel.SECURITY_WARNING));
        assertTrue(shouldShowBrowserControls(
                WEBAPP_URL, WEBAPP_URL + "/stuff.html", ConnectionSecurityLevel.SECURITY_WARNING));
    }

    /**
     * Convenience wrapper for
     * WebappDelegateFactory.BrowserControlsDelegate#shouldShowBrowserControls()
     */
    private static boolean shouldShowBrowserControls(
            String webappStartUrl, String url, int securityLevel) {
        return WebappDelegateFactory.BrowserControlsDelegate.shouldShowBrowserControls(
                webappStartUrl, url, securityLevel);
    }
}
