// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.SharedPreferences;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Tests for {@link UrlManager}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
    private static final double EPSILON = .001;
    private UrlManager mUrlManager = null;
    private MockPwsClient mMockPwsClient = null;

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);
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
    public void testAddUrlAfterClearAllUrlsWorks() {
        addPwsResult1();
        addPwsResult2();
        addPwsResult1();
        addPwsResult2();
        addUrlInfo1();
        addUrlInfo2();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        mUrlManager.clearAllUrls();

        // Add some more URLs...this should not crash if we cleared correctly.
        addUrlInfo1();
        addUrlInfo2();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        List<UrlInfo> urlInfos = mUrlManager.getUrls();
        assertEquals(2, urlInfos.size());
    }

    @Test
    public void testClearNearbyUrlsWorks() {
        addPwsResult1();
        addPwsResult2();
        addUrlInfo1();
        addUrlInfo2();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        mUrlManager.clearNearbyUrls();

        // Test that the URLs are not nearby, but do exist in the cache.
        List<UrlInfo> urlInfos = mUrlManager.getUrls(true);
        assertEquals(0, urlInfos.size());
        assertTrue(mUrlManager.containsInAnyCache(URL1));
        assertTrue(mUrlManager.containsInAnyCache(URL2));

        mUrlManager.clearAllUrls();

        // Test that cache is empty.
        assertFalse(mUrlManager.containsInAnyCache(URL1));
        assertFalse(mUrlManager.containsInAnyCache(URL2));
    }

    @Test
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
        assertFalse(mUrlManager.containsInAnyCache(URL1));
        assertTrue(mUrlManager.containsInAnyCache(URL1 + 1));
        assertTrue(mUrlManager.containsInAnyCache(URL1 + mUrlManager.getMaxCacheSize()));
    }

    @Test
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
        assertFalse(mUrlManager.containsInAnyCache(URL1));
        assertTrue(mUrlManager.containsInAnyCache(URL2));
    }

    @Test
    public void testAddUrlUpdatesCache() throws Exception {
        addEmptyPwsResult();
        addEmptyPwsResult();

        UrlInfo urlInfo = new UrlInfo(URL1);
        mUrlManager.addUrl(urlInfo);
        List<UrlInfo> urls = mUrlManager.getUrls(true);
        assertEquals(1, urls.size());
        assertEquals(urlInfo.getDistance(), urls.get(0).getDistance(), EPSILON);
        assertEquals(urlInfo.getDeviceAddress(), urls.get(0).getDeviceAddress());
        assertEquals(urlInfo.getFirstSeenTimestamp(), urls.get(0).getFirstSeenTimestamp());

        urlInfo = new UrlInfo(URL1).setDistance(100.0).setDeviceAddress("00:11:22:33:AA:BB");
        mUrlManager.addUrl(urlInfo);
        urls = mUrlManager.getUrls(true);
        assertEquals(1, urls.size());
        assertEquals(urlInfo.getDistance(), urls.get(0).getDistance(), EPSILON);
        assertEquals(urlInfo.getDeviceAddress(), urls.get(0).getDeviceAddress());
    }

    @Test
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
        assertFalse(mUrlManager.containsInAnyCache(URL1));
        assertTrue(mUrlManager.containsInAnyCache(URL2));
    }

    @Test
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
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Make sure URLs are in order and duplicates are omitted.
        List<UrlInfo> urlInfos = mUrlManager.getUrls();
        assertEquals(3, urlInfos.size());
        assertEquals(10.0, urlInfos.get(0).getDistance(), EPSILON);
        assertEquals(URL3, urlInfos.get(0).getUrl());
        assertEquals(30.0, urlInfos.get(1).getDistance(), EPSILON);
        assertEquals(URL1, urlInfos.get(1).getUrl());
        assertEquals(50.0, urlInfos.get(2).getDistance(), EPSILON);
        assertEquals(URL5, urlInfos.get(2).getUrl());
    }

    /*
     * Bug=crbug.com/684148
     */
    @Ignore
    @Test
    public void testSerializationWorksWithPoorlySerializedResult() throws Exception {
        addPwsResult1();
        addPwsResult2();
        long curTime = System.currentTimeMillis();
        mUrlManager.addUrl(new UrlInfo(URL1, 99.5, curTime + 42));
        mUrlManager.addUrl(new UrlInfo(URL2, 100.5, curTime + 43));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

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
        assertEquals(0, urlInfos.size());
        assertTrue(urlManager.containsInAnyCache(URL1));
        assertTrue(urlManager.containsInAnyCache(URL2));
    }

    @FlakyTest(message = "https://crbug.com/685471")
    @Test
    public void testSerializationWorksWithoutGarbageCollection() throws Exception {
        addPwsResult1();
        addPwsResult2();
        long curTime = System.currentTimeMillis();
        mUrlManager.addUrl(new UrlInfo(URL1, 99.5, curTime + 42));
        mUrlManager.addUrl(new UrlInfo(URL2, 100.5, curTime + 43));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Make sure all URLs are restored.
        UrlManager urlManager = new UrlManager();
        List<UrlInfo> urlInfos = urlManager.getUrls();
        assertEquals(0, urlInfos.size());
        assertTrue(urlManager.containsInAnyCache(URL1));
        assertTrue(urlManager.containsInAnyCache(URL2));
        Set<String> resolvedUrls = urlManager.getResolvedUrls();
        assertEquals(2, resolvedUrls.size());
    }

    @RetryOnFailure
    @Test
    public void testSerializationWorksWithGarbageCollection() throws Exception {
        addPwsResult1();
        addPwsResult2();
        mUrlManager.addUrl(new UrlInfo(URL1, 99.5, 42));
        mUrlManager.addUrl(new UrlInfo(URL2, 100.5, 43));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Make sure all URLs are restored.
        UrlManager urlManager = new UrlManager();
        List<UrlInfo> urlInfos = urlManager.getUrls();
        assertEquals(0, urlInfos.size());
        Set<String> resolvedUrls = urlManager.getResolvedUrls();
        assertEquals(0, resolvedUrls.size());
    }

    @Test
    public void testUpgradeFromNone() throws Exception {
        Set<String> oldResolvedUrls = new HashSet<String>();
        oldResolvedUrls.add("old");
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        prefs.edit()
                .remove(UrlManager.getVersionKey())
                .putStringSet("physicalweb_nearby_urls", oldResolvedUrls)
                .putInt("org.chromium.chrome.browser.physicalweb.VERSION", 1)
                .putInt("org.chromium.chrome.browser.physicalweb.BOTTOM_BAR_DISPLAY_COUNT", 1)
                .putBoolean("physical_web_ignore_other_clients", true)
                .apply();
        final Lock lock = new ReentrantLock();
        final Condition condition = lock.newCondition();
        prefs.registerOnSharedPreferenceChangeListener(
                new SharedPreferences.OnSharedPreferenceChangeListener() {
                    public void onSharedPreferenceChanged(
                            SharedPreferences sharedPreferences, String key) {
                        lock.lock();
                        try {
                            condition.signal();
                        } finally {
                            lock.unlock();
                        }
                    }
                });
        new UrlManager();
        lock.lock();
        try {
            assertTrue(condition.await(5, TimeUnit.SECONDS));
        } finally {
            lock.unlock();
        }

        // Make sure the new prefs are populated and old prefs are gone.
        SharedPreferences sharedPreferences = ContextUtils.getAppSharedPreferences();
        assertTrue(sharedPreferences.contains(UrlManager.getVersionKey()));
        assertEquals(
                UrlManager.getVersion(), sharedPreferences.getInt(UrlManager.getVersionKey(), 0));
        assertFalse(sharedPreferences.contains("physicalweb_nearby_urls"));
        assertFalse(sharedPreferences.contains("org.chromium.chrome.browser.physicalweb.VERSION"));
        assertFalse(sharedPreferences.contains(
                "org.chromium.chrome.browser.physicalweb.BOTTOM_BAR_DISPLAY_COUNT"));
        assertFalse(sharedPreferences.contains("physical_web_ignore_other_clients"));
    }
}
