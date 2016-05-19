// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.components.security_state.ConnectionSecurityLevel;

/**
 * Tests the logic in top controls visibility delegate in WebappActivity.
 */
public class WebappVisibilityTest extends WebappActivityTestBase {
    private static final String WEBAPP_URL = "http://originalwebsite.com";

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
    }

    @MediumTest
    @Feature({"Webapps"})
    public void testShouldShowTopControls() {
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
        assertFalse(getActivity().shouldShowTopControls(WEBAPP_URL, ConnectionSecurityLevel.NONE));
        assertFalse(getActivity().shouldShowTopControls(
                WEBAPP_URL + "/things.html", ConnectionSecurityLevel.NONE));
        assertFalse(getActivity().shouldShowTopControls(
                WEBAPP_URL + "/stuff.html", ConnectionSecurityLevel.NONE));

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
                WEBAPP_URL, ConnectionSecurityLevel.SECURITY_WARNING));
        assertTrue(getActivity().shouldShowTopControls(
                WEBAPP_URL + "/things.html", ConnectionSecurityLevel.SECURITY_WARNING));
        assertTrue(getActivity().shouldShowTopControls(
                WEBAPP_URL + "/stuff.html", ConnectionSecurityLevel.SECURITY_WARNING));
    }
}
