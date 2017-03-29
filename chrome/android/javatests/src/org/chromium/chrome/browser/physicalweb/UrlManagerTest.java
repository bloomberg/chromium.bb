// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.content.SharedPreferences;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Tests for {@link UrlManager}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class UrlManagerTest {
    private static final String URL1 = "https://example.com/";
    private static final String TITLE1 = "Example";
    private static final String DESC1 = "Example Website";
    private static final String URL2 = "https://google.com/";
    private static final String TITLE2 = "Google";
    private static final String DESC2 = "Search the Web";
    private static final String URL3 = "https://html5zombo.com/";
    private static final String URL4 = "https://hooli.xyz/";
    private static final String URL5 = "https://www.gmail.com/mail/help/paper/";
    private static final String GROUP1 = "group1";
    private static final String GROUP2 = "group2";
    private static final String GROUP3 = "group3";
    private static final String PREF_PHYSICAL_WEB = "physical_web";
    private static final int PHYSICAL_WEB_OFF = 0;
    private static final int PHYSICAL_WEB_ON = 1;
    private static final int PHYSICAL_WEB_ONBOARDING = 2;
    private UrlManager mUrlManager = null;
    private MockPwsClient mMockPwsClient = null;

    @Before
    public void setUp() throws Exception {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putInt(PREF_PHYSICAL_WEB, PHYSICAL_WEB_ON)
                .apply();
        UrlManager.clearPrefsForTesting();
        mUrlManager = new UrlManager();
        mMockPwsClient = new MockPwsClient();
        mUrlManager.overridePwsClientForTesting(mMockPwsClient);
    }

    private void addPwsResult1() {
        ArrayList<PwsResult> results = new ArrayList<>();
        results.add(new PwsResult(URL1, URL1, null, TITLE1, DESC1, GROUP1));
        mMockPwsClient.addPwsResults(results);
    }

    private void addPwsResult2() {
        ArrayList<PwsResult> results = new ArrayList<>();
        results.add(new PwsResult(URL2, URL2, null, TITLE2, DESC2, GROUP2));
        mMockPwsClient.addPwsResults(results);
    }

    private void addUrlInfo1() {
        mUrlManager.addUrl(new UrlInfo(URL1));
    }

    private void addUrlInfo2() {
        mUrlManager.addUrl(new UrlInfo(URL2));
    }

    private void removeUrlInfo1() {
        mUrlManager.removeUrl(new UrlInfo(URL1));
    }

    private void removeUrlInfo2() {
        mUrlManager.removeUrl(new UrlInfo(URL2));
    }

    private void addEmptyPwsResult() {
        mMockPwsClient.addPwsResults(new ArrayList<PwsResult>());
    }

    private void setOnboarding() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putInt(PREF_PHYSICAL_WEB, PHYSICAL_WEB_ONBOARDING)
                .apply();
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testAddUrlAfterClearAllUrlsWorks() {
        addPwsResult1();
        addPwsResult2();
        addPwsResult1();
        addPwsResult2();
        addUrlInfo1();
        addUrlInfo2();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mUrlManager.clearAllUrls();

        // Add some more URLs...this should not crash if we cleared correctly.
        addUrlInfo1();
        addUrlInfo2();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        List<UrlInfo> urlInfos = mUrlManager.getUrls();
        Assert.assertEquals(2, urlInfos.size());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testClearNearbyUrlsWorks() {
        addPwsResult1();
        addPwsResult2();
        addUrlInfo1();
        addUrlInfo2();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        mUrlManager.clearNearbyUrls();

        // Test that the URLs are not nearby, but do exist in the cache.
        List<UrlInfo> urlInfos = mUrlManager.getUrls(true);
        Assert.assertEquals(0, urlInfos.size());
        Assert.assertTrue(mUrlManager.containsInAnyCache(URL1));
        Assert.assertTrue(mUrlManager.containsInAnyCache(URL2));

        mUrlManager.clearAllUrls();

        // Test that cache is empty.
        Assert.assertFalse(mUrlManager.containsInAnyCache(URL1));
        Assert.assertFalse(mUrlManager.containsInAnyCache(URL2));
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testAddUrlGarbageCollectsForSize() throws Exception {
        // Add and remove 101 URLs, making sure one is clearly slightly older than the others.
        addEmptyPwsResult();
        UrlInfo urlInfo = new UrlInfo(URL1, -1.0, System.currentTimeMillis() - 1);
        mUrlManager.addUrl(urlInfo);
        mUrlManager.removeUrl(urlInfo);
        for (int i = 1; i <= mUrlManager.getMaxCacheSize(); i++) {
            mMockPwsClient.addPwsResults(new ArrayList<PwsResult>());
            urlInfo = new UrlInfo(URL1 + i, -1.0, System.currentTimeMillis());
            mUrlManager.addUrl(urlInfo);
            mUrlManager.removeUrl(urlInfo);
        }

        // Make our cache is missing the first URL and contains the others.
        Assert.assertFalse(mUrlManager.containsInAnyCache(URL1));
        Assert.assertTrue(mUrlManager.containsInAnyCache(URL1 + 1));
        Assert.assertTrue(mUrlManager.containsInAnyCache(URL1 + mUrlManager.getMaxCacheSize()));
    }

    @Test
    @SmallTest
    public void testAddUrlGarbageCollectsForAge() throws Exception {
        // Add a URL with a phony timestamp.
        addEmptyPwsResult();
        addEmptyPwsResult();
        UrlInfo urlInfo1 = new UrlInfo(URL1, -1.0, 0);
        UrlInfo urlInfo2 = new UrlInfo(URL2, -1.0, System.currentTimeMillis());
        mUrlManager.addUrl(urlInfo1);
        mUrlManager.removeUrl(urlInfo1);
        mUrlManager.addUrl(urlInfo2);
        mUrlManager.removeUrl(urlInfo2);

        // Make sure only URL2 is still in the cache.
        Assert.assertFalse(mUrlManager.containsInAnyCache(URL1));
        Assert.assertTrue(mUrlManager.containsInAnyCache(URL2));
    }

    @Test
    @SmallTest
    public void testAddUrlUpdatesCache() throws Exception {
        addEmptyPwsResult();
        addEmptyPwsResult();

        UrlInfo urlInfo = new UrlInfo(URL1);
        mUrlManager.addUrl(urlInfo);
        List<UrlInfo> urls = mUrlManager.getUrls(true);
        Assert.assertEquals(1, urls.size());
        Assert.assertEquals(urlInfo.getDistance(), urls.get(0).getDistance(), 0);
        Assert.assertEquals(urlInfo.getDeviceAddress(), urls.get(0).getDeviceAddress());
        Assert.assertEquals(urlInfo.getFirstSeenTimestamp(), urls.get(0).getFirstSeenTimestamp());

        urlInfo = new UrlInfo(URL1).setDistance(100.0).setDeviceAddress("00:11:22:33:AA:BB");
        mUrlManager.addUrl(urlInfo);
        urls = mUrlManager.getUrls(true);
        Assert.assertEquals(1, urls.size());
        Assert.assertEquals(urlInfo.getDistance(), urls.get(0).getDistance(), 0);
        Assert.assertEquals(urlInfo.getDeviceAddress(), urls.get(0).getDeviceAddress());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testAddUrlTwiceWorks() throws Exception {
        // Add and remove an old URL twice and add new URL twice before removing.
        // This should cover several issues involved with updating the cache queue.
        addEmptyPwsResult();
        addEmptyPwsResult();
        addEmptyPwsResult();
        addEmptyPwsResult();
        UrlInfo urlInfo1 = new UrlInfo(URL1, -1.0, 0);
        UrlInfo urlInfo2 = new UrlInfo(URL2, -1.0, System.currentTimeMillis());
        mUrlManager.addUrl(urlInfo1);
        mUrlManager.removeUrl(urlInfo1);
        mUrlManager.addUrl(urlInfo1);
        mUrlManager.removeUrl(urlInfo1);
        mUrlManager.addUrl(urlInfo2);
        mUrlManager.addUrl(urlInfo2);
        mUrlManager.removeUrl(urlInfo2);

        // Make sure only URL2 is still in the cache.
        Assert.assertFalse(mUrlManager.containsInAnyCache(URL1));
        Assert.assertTrue(mUrlManager.containsInAnyCache(URL2));
    }

    @Test
    @SmallTest
    public void testGetUrlsSortsAndDedups() throws Exception {
        // Construct results with matching group IDs and check that getUrls returns only the closest
        // URL in each group. The list should be sorted by distance, closest first.
        addPwsResult1(); // GROUP1
        addPwsResult2(); // GROUP2
        mMockPwsClient.addPwsResult(new PwsResult(URL3, URL2 + "#a", null, TITLE2, DESC2, GROUP2));
        mMockPwsClient.addPwsResult(new PwsResult(URL4, URL1, null, TITLE1, DESC1, GROUP1));
        mMockPwsClient.addPwsResult(new PwsResult(URL5, URL5, null, TITLE1, DESC1, GROUP3));
        mUrlManager.addUrl(new UrlInfo(URL1, 30.0, System.currentTimeMillis()));
        mUrlManager.addUrl(new UrlInfo(URL2, 20.0, System.currentTimeMillis()));
        mUrlManager.addUrl(new UrlInfo(URL3, 10.0, System.currentTimeMillis()));
        mUrlManager.addUrl(new UrlInfo(URL4, 40.0, System.currentTimeMillis()));
        mUrlManager.addUrl(new UrlInfo(URL5, 50.0, System.currentTimeMillis()));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Make sure URLs are in order and duplicates are omitted.
        List<UrlInfo> urlInfos = mUrlManager.getUrls();
        Assert.assertEquals(3, urlInfos.size());
        Assert.assertEquals(10.0, urlInfos.get(0).getDistance(), 0);
        Assert.assertEquals(URL3, urlInfos.get(0).getUrl());
        Assert.assertEquals(30.0, urlInfos.get(1).getDistance(), 0);
        Assert.assertEquals(URL1, urlInfos.get(1).getUrl());
        Assert.assertEquals(50.0, urlInfos.get(2).getDistance(), 0);
        Assert.assertEquals(URL5, urlInfos.get(2).getUrl());
    }

    /*
     * @SmallTest
     * Bug=crbug.com/684148
     */
    @Test
    @DisabledTest
    public void testSerializationWorksWithPoorlySerializedResult() throws Exception {
        addPwsResult1();
        addPwsResult2();
        long curTime = System.currentTimeMillis();
        mUrlManager.addUrl(new UrlInfo(URL1, 99.5, curTime + 42));
        mUrlManager.addUrl(new UrlInfo(URL2, 100.5, curTime + 43));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Create an invalid serialization.
        Set<String> serializedUrls = new HashSet<>();
        serializedUrls.add(new UrlInfo(URL1, 99.5, curTime + 42).jsonSerialize().toString());
        serializedUrls.add("{\"not_a_value\": \"This is totally not a serialized UrlInfo.\"}");
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putStringSet("physicalweb_all_urls", serializedUrls)
                .apply();

        // Make sure only the properly serialized URL is restored.
        UrlManager urlManager = new UrlManager();
        List<UrlInfo> urlInfos = urlManager.getUrls();
        Assert.assertEquals(0, urlInfos.size());
        Assert.assertTrue(urlManager.containsInAnyCache(URL1));
        Assert.assertTrue(urlManager.containsInAnyCache(URL2));
    }

    @Test
    @FlakyTest(message = "https://crbug.com/685471")
    @SmallTest
    @RetryOnFailure
    public void testSerializationWorksWithoutGarbageCollection() throws Exception {
        addPwsResult1();
        addPwsResult2();
        long curTime = System.currentTimeMillis();
        mUrlManager.addUrl(new UrlInfo(URL1, 99.5, curTime + 42));
        mUrlManager.addUrl(new UrlInfo(URL2, 100.5, curTime + 43));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Make sure all URLs are restored.
        UrlManager urlManager = new UrlManager();
        List<UrlInfo> urlInfos = urlManager.getUrls();
        Assert.assertEquals(0, urlInfos.size());
        Assert.assertTrue(urlManager.containsInAnyCache(URL1));
        Assert.assertTrue(urlManager.containsInAnyCache(URL2));
        Set<String> resolvedUrls = urlManager.getResolvedUrls();
        Assert.assertEquals(2, resolvedUrls.size());
    }

    @Test
    @SmallTest
    @RetryOnFailure
    public void testSerializationWorksWithGarbageCollection() throws Exception {
        addPwsResult1();
        addPwsResult2();
        mUrlManager.addUrl(new UrlInfo(URL1, 99.5, 42));
        mUrlManager.addUrl(new UrlInfo(URL2, 100.5, 43));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Make sure all URLs are restored.
        UrlManager urlManager = new UrlManager();
        List<UrlInfo> urlInfos = urlManager.getUrls();
        Assert.assertEquals(0, urlInfos.size());
        Set<String> resolvedUrls = urlManager.getResolvedUrls();
        Assert.assertEquals(0, resolvedUrls.size());
    }

    @Test
    @SmallTest
    public void testUpgradeFromNone() throws Exception {
        Set<String> oldResolvedUrls = new HashSet<String>();
        oldResolvedUrls.add("old");
        ContextUtils.getAppSharedPreferences()
                .edit()
                .remove(UrlManager.getVersionKey())
                .putStringSet("physicalweb_nearby_urls", oldResolvedUrls)
                .putInt("org.chromium.chrome.browser.physicalweb.VERSION", 1)
                .putInt("org.chromium.chrome.browser.physicalweb.BOTTOM_BAR_DISPLAY_COUNT", 1)
                .apply();
        new UrlManager();

        // Make sure the new prefs are populated and old prefs are gone.
        final SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
                return sharedPreferences.contains(UrlManager.getVersionKey())
                        && !sharedPreferences.contains("physicalweb_nearby_urls")
                        && !sharedPreferences.contains(
                                   "org.chromium.chrome.browser.physicalweb.VERSION")
                        && !sharedPreferences.contains("org.chromium.chrome.browser.physicalweb"
                                   + ".BOTTOM_BAR_DISPLAY_COUNT");
            }
        }, 5000, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        Assert.assertEquals(
                UrlManager.getVersion(), sharedPreferences.getInt(UrlManager.getVersionKey(), 0));
    }
}
