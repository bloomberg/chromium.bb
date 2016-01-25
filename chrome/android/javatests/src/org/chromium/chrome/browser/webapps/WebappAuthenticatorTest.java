// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.metrics.RecordHistogram;

public class WebappAuthenticatorTest extends InstrumentationTestCase {
    @SmallTest
    public void testAuthentication() {
        RecordHistogram.disableForTests();
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
