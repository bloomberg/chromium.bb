// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/**
 * Tests for {@link WebappDelegateFactory}.
 */
public class WebappVisibilityTest extends NativeLibraryTestBase {
    private static final String WEBAPP_URL = "http://originalwebsite.com";

    private static enum Type {
        WEBAPP,
        WEBAPK,
    };

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryNoBrowserProcess();
    }

    @MediumTest
    @Feature({"Webapps"})
    public void testShouldShowBrowserControls() {
        Type[] types = new Type[] {Type.WEBAPP, Type.WEBAPK};
        for (Type type : types) {
            boolean isWebApk = (type == Type.WEBAPK);

            // Show browser controls for out-of-domain URLs.
            assertTrue(shouldShowBrowserControls(WEBAPP_URL, "http://notoriginalwebsite.com",
                    ConnectionSecurityLevel.NONE, type));
            assertTrue(shouldShowBrowserControls(
                    WEBAPP_URL, "http://otherwebsite.com", ConnectionSecurityLevel.NONE, type));

            // Do not show browser controls for subpaths.
            assertFalse(shouldShowBrowserControls(
                    WEBAPP_URL, WEBAPP_URL, ConnectionSecurityLevel.NONE, type));
            assertFalse(shouldShowBrowserControls(
                    WEBAPP_URL, WEBAPP_URL + "/things.html", ConnectionSecurityLevel.NONE, type));
            assertFalse(shouldShowBrowserControls(
                    WEBAPP_URL, WEBAPP_URL + "/stuff.html", ConnectionSecurityLevel.NONE, type));

            // For WebAPKs but not Webapps show browser controls for subdomains and private
            // registries that are secure.
            assertEquals(isWebApk,
                    shouldShowBrowserControls(WEBAPP_URL, "http://sub.originalwebsite.com",
                            ConnectionSecurityLevel.NONE, type));
            assertEquals(isWebApk,
                    shouldShowBrowserControls(WEBAPP_URL, "http://thing.originalwebsite.com",
                            ConnectionSecurityLevel.NONE, type));

            // Do not show browser controls when URL is not available yet.
            assertFalse(
                    shouldShowBrowserControls(WEBAPP_URL, "", ConnectionSecurityLevel.NONE, type));

            // Show browser controls for non secure URLs.
            assertTrue(shouldShowBrowserControls(
                    WEBAPP_URL, WEBAPP_URL, ConnectionSecurityLevel.SECURITY_WARNING, type));
            assertTrue(shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL + "/things.html",
                    ConnectionSecurityLevel.SECURITY_WARNING, type));
            assertTrue(shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL + "/stuff.html",
                    ConnectionSecurityLevel.SECURITY_WARNING, type));
            assertTrue(shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL + "/stuff.html",
                    ConnectionSecurityLevel.DANGEROUS, type));
            assertTrue(shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL + "/things.html",
                    ConnectionSecurityLevel.DANGEROUS, type));
            assertTrue(shouldShowBrowserControls(WEBAPP_URL, "http://sub.originalwebsite.com",
                    ConnectionSecurityLevel.SECURITY_WARNING, type));
            assertTrue(shouldShowBrowserControls(
                    WEBAPP_URL, "http://notoriginalwebsite.com", ConnectionSecurityLevel.DANGEROUS, type));
            assertTrue(shouldShowBrowserControls(
                    WEBAPP_URL, "http://otherwebsite.com", ConnectionSecurityLevel.DANGEROUS, type));
            assertTrue(shouldShowBrowserControls(
                    WEBAPP_URL, "http://thing.originalwebsite.com", ConnectionSecurityLevel.DANGEROUS, type));
        }
    }

    /**
     * Calls either WebappBrowserControlsDelegate#shouldShowBrowserControls() or
     * WebApkBrowserControlsDelegate#shouldShowBrowserControls() based on the type.
     * @param webappStartUrlOrScopeUrl Web Manifest start URL when {@link type} == Type.WEBAPP and
     *                                 the Web Manifest scope URL otherwise.
     * @param url The current page URL
     * @param type
     */
    private static boolean shouldShowBrowserControls(
            String webappStartUrlOrScopeUrl, String url, int securityLevel, Type type) {
        WebappBrowserControlsDelegate delegate;
        WebappInfo info;
        if (type == Type.WEBAPP) {
            delegate = new WebappBrowserControlsDelegate(null, new Tab(0, false, null));
            info = WebappInfo.create(
                    "", webappStartUrlOrScopeUrl, null, null, null, null, 0, 0, 0, 0, 0, false);
        } else {
            delegate = new WebApkBrowserControlsDelegate(null, new Tab(0, false, null));
            info = WebApkInfo.create("", "", webappStartUrlOrScopeUrl, null, null, null, 0, 0, 0, 0,
                    0, "", 0, null, "", null);
        }
        return delegate.shouldShowBrowserControls(info, url, securityLevel);
    }
}
