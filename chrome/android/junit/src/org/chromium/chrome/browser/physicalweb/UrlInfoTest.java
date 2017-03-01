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
 * Tests for {@link UrlInfo}.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class UrlInfoTest {
    private static final String URL = "https://example.com";
    private static final double EPSILON = .001;
    UrlInfo mReferenceUrlInfo = null;
    JSONObject mReferenceJsonObject = null;

    @Before
    public void setUp() throws JSONException {
        mReferenceUrlInfo = new UrlInfo(URL, 99.5, 42)
                .setHasBeenDisplayed()
                .setDeviceAddress("00:11:22:33:AA:BB");
        // Because we can't print JSON sorted by keys, the order is important here.
        mReferenceJsonObject = new JSONObject("{"
                + "    \"url\": \"" + URL + "\","
                + "    \"distance\": 99.5,"
                + "    \"first_seen_timestamp\": 42,"
                + "    \"device_address\": \"00:11:22:33:AA:BB\","
                + "    \"has_been_displayed\": true"
                + "}");
    }

    @Test
    public void testJsonSerializeWorks() throws JSONException {
        assertEquals(mReferenceJsonObject.toString(), mReferenceUrlInfo.jsonSerialize().toString());
    }

    @Test
    public void testJsonDeserializeWorks() throws JSONException {
        UrlInfo urlInfo = UrlInfo.jsonDeserialize(mReferenceJsonObject);
        assertEquals(mReferenceUrlInfo.getUrl(), urlInfo.getUrl());
        assertEquals(mReferenceUrlInfo.getDistance(), urlInfo.getDistance(), EPSILON);
        assertEquals(mReferenceUrlInfo.getFirstSeenTimestamp(), urlInfo.getFirstSeenTimestamp());
        assertEquals(mReferenceUrlInfo.getDeviceAddress(), urlInfo.getDeviceAddress());
        assertEquals(mReferenceUrlInfo.hasBeenDisplayed(), urlInfo.hasBeenDisplayed());
    }
}
