// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import static org.junit.Assert.assertEquals;

import org.json.JSONException;
import org.json.JSONObject;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/**
 * Tests for {@link PwsResult}.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class PwsResultTest {
    PwsResult mReferencePwsResult = null;
    JSONObject mReferenceJsonObject = null;

    @Before
    public void setUp() throws Exception {
        mReferencePwsResult = new PwsResult("https://shorturl.com", "https://longurl.com",
                "https://longurl.com/favicon.ico", "This is a page", "Pages are the best",
                "group1");
        // Because we can't print JSON sorted by keys, the order is important here.
        mReferenceJsonObject = new JSONObject("{"
                + "    \"scannedUrl\": \"https://shorturl.com\","
                + "    \"resolvedUrl\": \"https://longurl.com\","
                + "    \"pageInfo\": {"
                + "        \"icon\": \"https://longurl.com/favicon.ico\","
                + "        \"title\": \"This is a page\","
                + "        \"description\": \"Pages are the best\","
                + "        \"groupId\": \"group1\""
                + "    }"
                + "}");
    }

    @Test
    public void testJsonSerializeWorks() throws JSONException {
        assertEquals(
                mReferenceJsonObject.toString(), mReferencePwsResult.jsonSerialize().toString());
    }

    @Test
    public void testJsonDeserializeWorks() throws JSONException {
        PwsResult pwsResult = PwsResult.jsonDeserialize(mReferenceJsonObject);
        assertEquals(mReferencePwsResult.requestUrl, pwsResult.requestUrl);
        assertEquals(mReferencePwsResult.siteUrl, pwsResult.siteUrl);
        assertEquals(mReferencePwsResult.iconUrl, pwsResult.iconUrl);
        assertEquals(mReferencePwsResult.title, pwsResult.title);
        assertEquals(mReferencePwsResult.description, pwsResult.description);
        assertEquals(mReferencePwsResult.groupId, pwsResult.groupId);
    }
}
