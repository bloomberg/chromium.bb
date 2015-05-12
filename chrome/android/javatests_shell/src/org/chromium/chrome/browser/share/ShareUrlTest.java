// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Intent;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;

/**
 * Tests sharing URLs in reader mode (DOM distiller)
 */
public class ShareUrlTest extends ChromeShellTestBase {
    private static final String HTTP_URL = "http://www.google.com/";
    private static final String HTTPS_URL = "https://www.google.com/";

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        // load native methods in DomDistillerUrlUtils
        startChromeBrowserProcessSync(getInstrumentation().getTargetContext());
    }

    private void assertCorrectUrl(String originalUrl, String sharedUrl) {
        Intent intent = ShareHelper.getShareIntent("", sharedUrl, null);
        assert (intent.hasExtra(Intent.EXTRA_TEXT));
        String url = intent.getStringExtra(Intent.EXTRA_TEXT);
        assertEquals(originalUrl, url);
    }

    @SmallTest
    public void testNormalUrl() {
        assertCorrectUrl(HTTP_URL, HTTP_URL);
        assertCorrectUrl(HTTPS_URL, HTTPS_URL);
    }

    @SmallTest
    public void testDistilledUrl() {
        final String DomDistillerScheme = "chrome-distiller";
        String distilledHttpUrl =
                DomDistillerUrlUtils.getDistillerViewUrlFromUrl(DomDistillerScheme, HTTP_URL);
        String distilledHttpsUrl =
                DomDistillerUrlUtils.getDistillerViewUrlFromUrl(DomDistillerScheme, HTTPS_URL);

        assertCorrectUrl(HTTP_URL, distilledHttpUrl);
        assertCorrectUrl(HTTPS_URL, distilledHttpsUrl);
    }
}
