// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.support.test.filters.MediumTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content.browser.test.NativeLibraryTestRule;

/**
 * Tests for {@link WebappDelegateFactory}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebappVisibilityTest {
    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    @Rule
    public UiThreadTestRule mUiThreadTestRule = new UiThreadTestRule();

    private static final String WEBAPP_URL = "http://originalwebsite.com";

    private static enum Type {
        WEBAPP,
        WEBAPK,
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testShouldShowBrowserControls() throws Throwable {
        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                for (Type type : new Type[] {Type.WEBAPP, Type.WEBAPK}) {
                    testCanAutoHideBrowserControls(type);
                    for (int displayMode : new int[] {WebDisplayMode.STANDALONE,
                            WebDisplayMode.FULLSCREEN, WebDisplayMode.MINIMAL_UI}) {
                        testShouldShowBrowserControls(type, displayMode);
                    }
                }
            }
        });
    }

    private static void testCanAutoHideBrowserControls(Type type) {
        // Allow auto-hiding controls unless we're on a dangerous connection.
        Assert.assertTrue(canAutoHideBrowserControls(type, ConnectionSecurityLevel.NONE));
        Assert.assertTrue(canAutoHideBrowserControls(type, ConnectionSecurityLevel.SECURE));
        Assert.assertTrue(canAutoHideBrowserControls(type, ConnectionSecurityLevel.EV_SECURE));
        Assert.assertTrue(
                canAutoHideBrowserControls(type, ConnectionSecurityLevel.HTTP_SHOW_WARNING));
        Assert.assertFalse(canAutoHideBrowserControls(type, ConnectionSecurityLevel.DANGEROUS));
    }

    private static void testShouldShowBrowserControls(Type type, @WebDisplayMode int displayMode) {
        // Show browser controls for out-of-domain URLs.
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, "http://notoriginalwebsite.com",
                ConnectionSecurityLevel.NONE, type, displayMode));
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, "http://otherwebsite.com",
                ConnectionSecurityLevel.NONE, type, displayMode));

        // Do not show browser controls for subpaths, unless using Minimal-UI.
        Assert.assertEquals(displayMode == WebDisplayMode.MINIMAL_UI,
                shouldShowBrowserControls(
                        WEBAPP_URL, WEBAPP_URL, ConnectionSecurityLevel.NONE, type, displayMode));
        Assert.assertEquals(displayMode == WebDisplayMode.MINIMAL_UI,
                shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL + "/things.html",
                        ConnectionSecurityLevel.NONE, type, displayMode));
        Assert.assertEquals(displayMode == WebDisplayMode.MINIMAL_UI,
                shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL + "/stuff.html",
                        ConnectionSecurityLevel.NONE, type, displayMode));

        // For WebAPKs but not Webapps show browser controls for subdomains and private
        // registries that are secure.
        Assert.assertEquals(type == Type.WEBAPK || displayMode == WebDisplayMode.MINIMAL_UI,
                shouldShowBrowserControls(WEBAPP_URL, "http://sub.originalwebsite.com",
                        ConnectionSecurityLevel.NONE, type, displayMode));
        Assert.assertEquals(type == Type.WEBAPK || displayMode == WebDisplayMode.MINIMAL_UI,
                shouldShowBrowserControls(WEBAPP_URL, "http://thing.originalwebsite.com",
                        ConnectionSecurityLevel.NONE, type, displayMode));

        // Do not show browser controls when URL is not available yet.
        Assert.assertFalse(shouldShowBrowserControls(
                        WEBAPP_URL, "", ConnectionSecurityLevel.NONE, type, displayMode));

        // Show browser controls for Dangerous URLs.
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL,
                ConnectionSecurityLevel.DANGEROUS, type, displayMode));
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL + "/stuff.html",
                ConnectionSecurityLevel.DANGEROUS, type, displayMode));
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL + "/things.html",
                ConnectionSecurityLevel.DANGEROUS, type, displayMode));
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, "http://notoriginalwebsite.com",
                ConnectionSecurityLevel.DANGEROUS, type, displayMode));
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, "http://otherwebsite.com",
                ConnectionSecurityLevel.DANGEROUS, type, displayMode));
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, "http://thing.originalwebsite.com",
                ConnectionSecurityLevel.DANGEROUS, type, displayMode));
    }

    private static boolean shouldShowBrowserControls(String webappStartUrlOrScopeUrl, String url,
            int securityLevel, Type type, @WebDisplayMode int displayMode) {
        return createDelegate(type).shouldShowBrowserControls(
                createWebappInfo(webappStartUrlOrScopeUrl, type, displayMode), url, securityLevel);
    }

    private static boolean canAutoHideBrowserControls(Type type, int securityLevel) {
        return createDelegate(type).canAutoHideBrowserControls(securityLevel);
    }

    private static WebappBrowserControlsDelegate createDelegate(Type type) {
        return type == Type.WEBAPP
                ? new WebappBrowserControlsDelegate(null, new Tab(0, false, null))
                : new WebApkBrowserControlsDelegate(null, new Tab(0, false, null));
    }

    private static WebappInfo createWebappInfo(
            String webappStartUrlOrScopeUrl, Type type, @WebDisplayMode int displayMode) {
        return type == Type.WEBAPP
                ? WebappInfo.create("", webappStartUrlOrScopeUrl, null, null, null, null,
                          displayMode, 0, 0, 0, 0, false /* isIconGenerated */,
                          false /* forceNavigation */)
                : WebApkInfo.create("", "", webappStartUrlOrScopeUrl, null, null, null, null,
                          displayMode, 0, 0, 0, 0, "", 0, null, "", null,
                          false /* forceNavigation */);
    }
}
