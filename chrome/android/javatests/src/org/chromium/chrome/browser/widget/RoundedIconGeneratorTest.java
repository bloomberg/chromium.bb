// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.support.test.filters.SmallTest;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/**
 * Unit tests for RoundedIconGenerator.
 */
public class RoundedIconGeneratorTest extends NativeLibraryTestBase {
    private String getIconTextForUrl(String url, boolean includePrivateRegistries) {
        return RoundedIconGenerator.getIconTextForUrl(url, includePrivateRegistries);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        CommandLine.init(null);
        loadNativeLibraryAndInitBrowserProcess();
    }

    /**
     * Verifies that RoundedIconGenerator's ability to generate icons based on URLs considers the
     * appropriate parts of the URL for the icon to generate.
     */
    @SmallTest
    @Feature({"Browser", "RoundedIconGenerator"})
    public void testGetIconTextForUrl() {
        // Verify valid domains when including private registries.
        assertEquals("google.com", getIconTextForUrl("https://google.com/", true));
        assertEquals("google.com", getIconTextForUrl("https://www.google.com:443/", true));
        assertEquals("google.com", getIconTextForUrl("https://mail.google.com/", true));
        assertEquals("foo.appspot.com", getIconTextForUrl("https://foo.appspot.com/", true));

        // Verify valid domains when not including private registries.
        assertEquals("appspot.com", getIconTextForUrl("https://foo.appspot.com/", false));

        // Verify Chrome-internal
        assertEquals("chrome", getIconTextForUrl("chrome://about", false));
        assertEquals("chrome", getIconTextForUrl("chrome-native://newtab", false));

        // Verify that other URIs from which a hostname can be resolved use that.
        assertEquals("localhost", getIconTextForUrl("http://localhost/", false));
        assertEquals("google-chrome", getIconTextForUrl("https://google-chrome/", false));
        assertEquals("127.0.0.1", getIconTextForUrl("http://127.0.0.1/", false));

        // Verify that the fallback is the the URL itself.
        assertEquals("file:///home/chrome/test.html",
                getIconTextForUrl("file:///home/chrome/test.html", false));
        assertEquals("data:image", getIconTextForUrl("data:image", false));
    }
}
