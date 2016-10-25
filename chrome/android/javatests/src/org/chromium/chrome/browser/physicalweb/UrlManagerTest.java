// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.content.Context;
import android.content.SharedPreferences;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Tests for {@link UrlManager}.
 */
public class UrlManagerTest extends InstrumentationTestCase {
    private static final String URL1 = "https://example.com/";
    private static final String TITLE1 = "Example";
    private static final String DESC1 = "Example Website";
    private static final String URL2 = "https://google.com/";
    private static final String TITLE2 = "Google";
    private static final String DESC2 = "Search the Web";
    private static final String URL3 = "https://html5zombo.com/";
    private static final String PREF_PHYSICAL_WEB = "physical_web";
    private static final int PHYSICAL_WEB_OFF = 0;
    private static final int PHYSICAL_WEB_ON = 1;
    private static final int PHYSICAL_WEB_ONBOARDING = 2;
    private UrlManager mUrlManager = null;
    private MockPwsClient mMockPwsClient = null;
    private MockNotificationManagerProxy mMockNotificationManagerProxy = null;
    private SharedPreferences mSharedPreferences = null;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        mSharedPreferences = ContextUtils.getAppSharedPreferences();
        mSharedPreferences.edit().putInt(PREF_PHYSICAL_WEB, PHYSICAL_WEB_ON).apply();
        UrlManager.clearPrefsForTesting(context);
        mUrlManager = new UrlManager(context);
        mMockPwsClient = new MockPwsClient();
        mUrlManager.overridePwsClientForTesting(mMockPwsClient);
        mMockNotificationManagerProxy = new MockNotificationManagerProxy();
        mUrlManager.overrideNotificationManagerForTesting(mMockNotificationManagerProxy);
    }

    private void addPwsResult1() {
        ArrayList<PwsResult> results = new ArrayList<>();
        results.add(new PwsResult(URL1, URL1, null, TITLE1, DESC1, null));
        mMockPwsClient.addPwsResults(results);
    }

    private void addPwsResult2() {
        ArrayList<PwsResult> results = new ArrayList<>();
        results.add(new PwsResult(URL2, URL2, null, TITLE2, DESC2, null));
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
        mSharedPreferences.edit().putInt(PREF_PHYSICAL_WEB, PHYSICAL_WEB_ONBOARDING).apply();
    }

    @SmallTest
    @RetryOnFailure
    public void testAddUrlAfterClearAllUrlsWorks() {
        addPwsResult1();
        addPwsResult2();
        addPwsResult1();
        addPwsResult2();
        addUrlInfo1();
        addUrlInfo2();
        getInstrumentation().waitForIdleSync();
        mUrlManager.clearAllUrls();

        // Add some more URLs...this should not crash if we cleared correctly.
        addUrlInfo1();
        addUrlInfo2();
        getInstrumentation().waitForIdleSync();
        List<UrlInfo> urlInfos = mUrlManager.getUrls();
        assertEquals(2, urlInfos.size());
    }

    @SmallTest
    @RetryOnFailure
    public void testClearNearbyUrlsWorks() {
        addPwsResult1();
        addPwsResult2();
        addUrlInfo1();
        addUrlInfo2();
        getInstrumentation().waitForIdleSync();

        // Make sure that a notification was shown.
        List<NotificationEntry> notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(1, notifications.size());

        mUrlManager.clearNearbyUrls();

        // Test that the URLs are not nearby, but do exist in the cache.
        List<UrlInfo> urlInfos = mUrlManager.getUrls(true);
        assertEquals(0, urlInfos.size());
        assertTrue(mUrlManager.containsInAnyCache(URL1));
        assertTrue(mUrlManager.containsInAnyCache(URL2));

        // Make sure no notification is shown.
        notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(0, notifications.size());

        mUrlManager.clearAllUrls();

        // Test that cache is empty.
        assertFalse(mUrlManager.containsInAnyCache(URL1));
        assertFalse(mUrlManager.containsInAnyCache(URL2));
    }

    @SmallTest
    public void testAddUrlWhileOnboardingMakesNotification() throws Exception {
        setOnboarding();
        addPwsResult1();
        addUrlInfo1();
        getInstrumentation().waitForIdleSync();

        // Make sure that a resolution was *not* attempted.
        List<Collection<UrlInfo>> resolveCalls = mMockPwsClient.getResolveCalls();
        assertEquals(0, resolveCalls.size());

        // Make sure that we have no resolved URLs.
        List<UrlInfo> urls = mUrlManager.getUrls();
        assertEquals(0, urls.size());

        // Make sure that a notification was shown.
        List<NotificationEntry> notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(1, notifications.size());
    }

    @SmallTest
    @RetryOnFailure
    public void testAddUrlNoResolutionDoesNothing() throws Exception {
        addEmptyPwsResult();
        addUrlInfo1();
        getInstrumentation().waitForIdleSync();

        // Make sure that a resolution was attempted.
        List<Collection<UrlInfo>> resolveCalls = mMockPwsClient.getResolveCalls();
        assertEquals(1, resolveCalls.size());

        // Make sure that we have no resolved URLs.
        List<UrlInfo> urls = mUrlManager.getUrls();
        assertEquals(0, urls.size());
        // Make sure that we do have unresolved URLs.
        urls = mUrlManager.getUrls(true);
        assertEquals(1, urls.size());

        // Make sure that a notification was not shown.
        List<NotificationEntry> notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(0, notifications.size());
    }

    @SmallTest
    public void testAddUrlWithResolutionMakesNotification() throws Exception {
        addPwsResult1();
        addUrlInfo1();
        getInstrumentation().waitForIdleSync();

        // Make sure that a resolution was attempted.
        List<Collection<UrlInfo>> resolveCalls = mMockPwsClient.getResolveCalls();
        assertEquals(1, resolveCalls.size());

        // Make sure that we have our resolved URLs.
        List<UrlInfo> urls = mUrlManager.getUrls();
        assertEquals(1, urls.size());

        // Make sure that a notification was shown.
        List<NotificationEntry> notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(1, notifications.size());
    }

    @SmallTest
    @RetryOnFailure
    public void testAddTwoUrlsMakesOneNotification() throws Exception {
        addPwsResult1();
        addPwsResult2();

        // Adding one URL should fire a notification.
        addUrlInfo1();
        getInstrumentation().waitForIdleSync();
        assertEquals(1, mMockNotificationManagerProxy.getNotifications().size());

        // Adding a second should not.
        mMockNotificationManagerProxy.cancelAll();
        addUrlInfo2();
        assertEquals(0, mMockNotificationManagerProxy.getNotifications().size());
    }

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
        assertFalse(mUrlManager.containsInAnyCache(URL1));
        assertTrue(mUrlManager.containsInAnyCache(URL1 + 1));
        assertTrue(mUrlManager.containsInAnyCache(URL1 + mUrlManager.getMaxCacheSize()));
    }

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
        assertFalse(mUrlManager.containsInAnyCache(URL1));
        assertTrue(mUrlManager.containsInAnyCache(URL2));
    }

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
        assertFalse(mUrlManager.containsInAnyCache(URL1));
        assertTrue(mUrlManager.containsInAnyCache(URL2));
    }

    @SmallTest
    @FlakyTest  // crbug.com/622005
    public void testAddUrlInCacheWithOthersMakesNoNotification() throws Exception {
        addPwsResult1();
        addPwsResult2();
        addPwsResult1();
        addUrlInfo1();
        addUrlInfo2();
        removeUrlInfo1();
        getInstrumentation().waitForIdleSync();

        // Make sure the cache is in the appropriate state
        assertTrue(mUrlManager.containsInAnyCache(URL1));

        mMockNotificationManagerProxy.cancelAll();
        addUrlInfo1();
        getInstrumentation().waitForIdleSync();

        // Make sure that no notification is shown.
        List<NotificationEntry> notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(0, notifications.size());
    }

    @SmallTest
    @RetryOnFailure
    public void testAddUrlInCacheWithNoOthersMakesNotification() throws Exception {
        addPwsResult1();
        addPwsResult1();
        addUrlInfo1();
        removeUrlInfo1();
        getInstrumentation().waitForIdleSync();
        mMockNotificationManagerProxy.cancelAll();
        addUrlInfo1();
        getInstrumentation().waitForIdleSync();

        // Make sure that a notification was shown.
        List<NotificationEntry> notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(1, notifications.size());
    }

    @SmallTest
    public void testAddUrlNotInCacheWithOthersMakesNotification() throws Exception {
        addPwsResult1();
        addPwsResult2();
        addUrlInfo1();
        getInstrumentation().waitForIdleSync();
        mMockNotificationManagerProxy.cancelAll();
        addUrlInfo2();
        getInstrumentation().waitForIdleSync();

        // Make sure that a notification was shown.
        List<NotificationEntry> notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(1, notifications.size());
    }

    @SmallTest
    public void testRemoveOnlyUrlClearsNotification() throws Exception {
        addPwsResult1();
        addUrlInfo1();
        getInstrumentation().waitForIdleSync();

        // Make sure that a notification was shown.
        List<NotificationEntry> notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(1, notifications.size());

        removeUrlInfo1();

        // Make sure the URL was removed.
        List<UrlInfo> urls = mUrlManager.getUrls(true);
        assertEquals(0, urls.size());

        // Make sure no notification is shown.
        notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(0, notifications.size());
    }

    @SmallTest
    public void testClearAllUrlsClearsNotification() throws Exception {
        addPwsResult1();
        addUrlInfo1();
        getInstrumentation().waitForIdleSync();

        // Make sure that a notification was shown.
        List<NotificationEntry> notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(1, notifications.size());

        mUrlManager.clearAllUrls();

        // Make sure all URLs were removed.
        List<UrlInfo> urls = mUrlManager.getUrls(true);
        assertEquals(0, urls.size());

        // Make sure no notification is shown.
        notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(0, notifications.size());
    }

    @SmallTest
    @RetryOnFailure
    public void testGetUrlSorts() throws Exception {
        addEmptyPwsResult();
        addEmptyPwsResult();
        addEmptyPwsResult();
        UrlInfo urlInfo1 = new UrlInfo(URL1, 30.0, System.currentTimeMillis());
        UrlInfo urlInfo2 = new UrlInfo(URL2, 10.0, System.currentTimeMillis());
        UrlInfo urlInfo3 = new UrlInfo(URL3, 20.0, System.currentTimeMillis());
        mUrlManager.addUrl(urlInfo1);
        mUrlManager.addUrl(urlInfo2);
        mUrlManager.addUrl(urlInfo3);
        getInstrumentation().waitForIdleSync();

        // Make sure URLs are in order.
        List<UrlInfo> urlInfos = mUrlManager.getUrls(true);
        assertEquals(3, urlInfos.size());
        assertEquals(10.0, urlInfos.get(0).getDistance());
        assertEquals(URL2, urlInfos.get(0).getUrl());
        assertEquals(20.0, urlInfos.get(1).getDistance());
        assertEquals(URL3, urlInfos.get(1).getUrl());
        assertEquals(30.0, urlInfos.get(2).getDistance());
        assertEquals(URL1, urlInfos.get(2).getUrl());
    }

    @SmallTest
    @RetryOnFailure
    public void testSerializationWorksWithoutGarbageCollection() throws Exception {
        addPwsResult1();
        addPwsResult2();
        long curTime = System.currentTimeMillis();
        mUrlManager.addUrl(new UrlInfo(URL1, 99.5, curTime + 42));
        mUrlManager.addUrl(new UrlInfo(URL2, 100.5, curTime + 43));
        getInstrumentation().waitForIdleSync();

        // Make sure all URLs are restored.
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        UrlManager urlManager = new UrlManager(context);
        List<UrlInfo> urlInfos = urlManager.getUrls();
        assertEquals(2, urlInfos.size());
        Set<String> resolvedUrls = urlManager.getResolvedUrls();
        assertEquals(2, resolvedUrls.size());
    }

    @SmallTest
    @RetryOnFailure
    public void testSerializationWorksWithGarbageCollection() throws Exception {
        addPwsResult1();
        addPwsResult2();
        mUrlManager.addUrl(new UrlInfo(URL1, 99.5, 42));
        mUrlManager.addUrl(new UrlInfo(URL2, 100.5, 43));
        getInstrumentation().waitForIdleSync();

        // Make sure all URLs are restored.
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        UrlManager urlManager = new UrlManager(context);
        List<UrlInfo> urlInfos = urlManager.getUrls();
        assertEquals(0, urlInfos.size());
        Set<String> resolvedUrls = urlManager.getResolvedUrls();
        assertEquals(0, resolvedUrls.size());
    }

    @SmallTest
    public void testUpgradeFromNone() throws Exception {
        Set<String> oldResolvedUrls = new HashSet<String>();
        oldResolvedUrls.add("old");
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        mSharedPreferences.edit()
                .remove(UrlManager.getVersionKey())
                .putStringSet("physicalweb_resolved_urls", oldResolvedUrls)
                .apply();
        new UrlManager(context);

        // Make sure the new prefs are populated and old prefs are gone.
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mSharedPreferences.contains(UrlManager.getVersionKey())
                        && !mSharedPreferences.contains("physicalweb_resolved_urls");
            }
        }, 5000, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        assertEquals(UrlManager.getVersion(),
                mSharedPreferences.getInt(UrlManager.getVersionKey(), 0));
    }
}
