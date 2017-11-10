// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnerbookmarks;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for {@link PartnerBookmarksCache}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PartnerBookmarksCacheTest {
    private static final String TEST_PREFERENCES_NAME = "partner_bookmarks_favicon_cache_test";

    private PartnerBookmarksCache mCache;

    @Before
    public void setUp() throws Exception {
        mCache = new PartnerBookmarksCache(RuntimeEnvironment.application, TEST_PREFERENCES_NAME);
    }

    @After
    public void tearDown() throws Exception {
        mCache.clearCache();
    }

    @Test
    public void testReadEmptyCache() {
        Assert.assertTrue(mCache.read().isEmpty());
    }

    @Test
    public void testReadWrite() {
        Map<String, Long> inMap = new HashMap<String, Long>();
        inMap.put("url1", 1L);
        inMap.put("url2", 2L);
        mCache.write(inMap);

        Map<String, Long> readMap = mCache.read();
        Assert.assertEquals(readMap.keySet().size(), 2);
        Assert.assertTrue(readMap.keySet().containsAll(inMap.keySet()));
    }

    @Test(expected = IllegalArgumentException.class)
    public void testWriteNull() {
        mCache.write(null);
    }

    @Test
    public void testWriteEmptyMap() {
        Map<String, Long> inMap = new HashMap<String, Long>();
        inMap.put("url1", 1L);
        inMap.put("url2", 2L);
        mCache.write(inMap);

        mCache.write(new HashMap<String, Long>());
        Map<String, Long> readMap = mCache.read();
        Assert.assertEquals(readMap.keySet().size(), 0);
    }
}