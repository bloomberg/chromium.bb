// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;

/**
 * Tests for {@link WebappAuthenticator}.
 */
public class WebappAuthenticatorTest extends InstrumentationTestCase {
    @Override
    public void setUp() throws Exception {
        super.setUp();
        RecordHistogram.setDisabledForTests(true);
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        RecordHistogram.setDisabledForTests(false);
    }

    @SmallTest
    @Feature({"Webapps"})
    public void testAuthentication() {
        Context context = getInstrumentation().getTargetContext();
        String url = "http://www.example.org/hello.html";
        byte[] mac = WebappAuthenticator.getMacForUrl(context, url);
        assertNotNull(mac);
        assertTrue(WebappAuthenticator.isUrlValid(context, url, mac));
        assertFalse(WebappAuthenticator.isUrlValid(context, url + "?goats=true", mac));
        mac[4] += (byte) 1;
        assertFalse(WebappAuthenticator.isUrlValid(context, url, mac));
    }
}
