// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Tests for {@link UrlInfo}.
 */
public class UrlInfoTest extends TestCase {
    private static final String URL = "https://example.com";
    UrlInfo mReferenceUrlInfo = null;
    JSONObject mReferenceJsonObject = null;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mReferenceUrlInfo = new UrlInfo(URL, 99.5, 42);
        mReferenceUrlInfo.setHasBeenDisplayed();
        // Because we can't print JSON sorted by keys, the order is important here.
        mReferenceJsonObject = new JSONObject("{"
                + "    \"url\": \"" + URL + "\","
                + "    \"distance\": 99.5,"
                + "    \"scan_timestamp\": 42,"
                + "    \"has_been_displayed\": true"
                + "}");
    }

    @SmallTest
    public void testJsonSerializeWorks() throws JSONException {
        assertEquals(mReferenceJsonObject.toString(), mReferenceUrlInfo.jsonSerialize().toString());
    }

    @SmallTest
    public void testJsonDeserializeWorks() throws JSONException {
        assertEquals(mReferenceUrlInfo, UrlInfo.jsonDeserialize(mReferenceJsonObject));
    }
}
