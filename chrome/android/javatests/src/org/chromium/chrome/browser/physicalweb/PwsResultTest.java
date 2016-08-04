// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Tests for {@link PwsResult}.
 */
public class PwsResultTest extends TestCase {
    PwsResult mReferencePwsResult = null;
    JSONObject mReferenceJsonObject = null;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mReferencePwsResult = new PwsResult(
                "https://shorturl.com",
                "https://longurl.com",
                "https://longurl.com/favicon.ico",
                "This is a page",
                "Pages are the best",
                "group1");
        // Because we can't print JSON sorted by keys, the order is important here.
        mReferenceJsonObject = new JSONObject("{"
                + "    \"scannedUrl\": \"https://shorturl.com\","
                + "    \"resolvedUrl\": \"https://longurl.com\","
                + "    \"icon\": \"https://longurl.com/favicon.ico\","
                + "    \"title\": \"This is a page\","
                + "    \"description\": \"Pages are the best\","
                + "    \"group\": \"group1\""
                + "}");
    }

    @SmallTest
    public void testJsonSerializeWorks() throws JSONException {
        assertEquals(mReferenceJsonObject.toString(),
                mReferencePwsResult.jsonSerialize().toString());
    }

    @SmallTest
    public void testJsonDeserializeWorks() throws JSONException {
        PwsResult pwsResult = PwsResult.jsonDeserialize(mReferenceJsonObject);
        assertEquals(mReferencePwsResult.requestUrl, pwsResult.requestUrl);
        assertEquals(mReferencePwsResult.responseUrl, pwsResult.responseUrl);
        assertEquals(mReferencePwsResult.iconUrl, pwsResult.iconUrl);
        assertEquals(mReferencePwsResult.title, pwsResult.title);
        assertEquals(mReferencePwsResult.description, pwsResult.description);
        assertEquals(mReferencePwsResult.group, pwsResult.group);
    }
}
