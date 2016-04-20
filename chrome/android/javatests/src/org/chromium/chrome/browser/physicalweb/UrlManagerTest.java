// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy.NotificationEntry;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * Tests for the UrlManager class.
 */
public class UrlManagerTest extends InstrumentationTestCase {
    static final String URL1 = "https://example.com/";
    static final String TITLE1 = "Example";
    static final String DESC1 = "Example Website";
    static final String PREF_PHYSICAL_WEB = "physical_web";
    static final int PHYSICAL_WEB_OFF = 0;
    static final int PHYSICAL_WEB_ON = 1;
    static final int PHYSICAL_WEB_ONBOARDING = 2;
    UrlManager mUrlManager = null;
    MockPwsClient mMockPwsClient = null;
    MockNotificationManagerProxy mMockNotificationManagerProxy = null;
    SharedPreferences mSharedPreferences = null;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        mSharedPreferences = PreferenceManager.getDefaultSharedPreferences(context);
        mSharedPreferences.edit().putInt(PREF_PHYSICAL_WEB, PHYSICAL_WEB_ON).apply();
        UrlManager.clearPrefsForTesting(context);
        mUrlManager = new UrlManager(context);
        mMockPwsClient = new MockPwsClient();
        mUrlManager.overridePwsClientForTesting(mMockPwsClient);
        mMockNotificationManagerProxy = new MockNotificationManagerProxy();
        mUrlManager.overrideNotificationManagerForTesting(mMockNotificationManagerProxy);
    }

    private void setOnboarding() {
        mSharedPreferences.edit().putInt(PREF_PHYSICAL_WEB, PHYSICAL_WEB_ONBOARDING).apply();
    }

    @SmallTest
    public void testAddUrlWhileOnboardingMakesNotification() throws Exception {
        setOnboarding();
        ArrayList<PwsResult> results = new ArrayList<>();
        results.add(new PwsResult(URL1, URL1, null, TITLE1, DESC1));
        mMockPwsClient.addPwsResults(results);
        mUrlManager.addUrl(URL1);
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
    public void testAddUrlNoResolutionDoesNothing() throws Exception {
        mMockPwsClient.addPwsResults(new ArrayList<PwsResult>());
        mUrlManager.addUrl(URL1);
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
        ArrayList<PwsResult> results = new ArrayList<>();
        results.add(new PwsResult(URL1, URL1, null, TITLE1, DESC1));
        mMockPwsClient.addPwsResults(results);
        mUrlManager.addUrl(URL1);
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
    public void testRemoveOnlyUrlClearsNotification() throws Exception {
        ArrayList<PwsResult> results = new ArrayList<>();
        results.add(new PwsResult(URL1, URL1, null, TITLE1, DESC1));
        mMockPwsClient.addPwsResults(results);
        mUrlManager.addUrl(URL1);
        getInstrumentation().waitForIdleSync();

        // Make sure that a notification was shown.
        List<NotificationEntry> notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(1, notifications.size());

        mUrlManager.removeUrl(URL1);

        // Make sure the URL was removed.
        List<UrlInfo> urls = mUrlManager.getUrls(true);
        assertEquals(0, urls.size());

        // Make sure no notification is shown.
        notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(0, notifications.size());
    }

    @SmallTest
    public void testClearUrlsClearsNotification() throws Exception {
        ArrayList<PwsResult> results = new ArrayList<>();
        results.add(new PwsResult(URL1, URL1, null, TITLE1, DESC1));
        mMockPwsClient.addPwsResults(results);
        mUrlManager.addUrl(URL1);
        getInstrumentation().waitForIdleSync();

        // Make sure that a notification was shown.
        List<NotificationEntry> notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(1, notifications.size());

        mUrlManager.clearUrls();

        // Make sure all URLs were removed.
        List<UrlInfo> urls = mUrlManager.getUrls(true);
        assertEquals(0, urls.size());

        // Make sure no notification is shown.
        notifications = mMockNotificationManagerProxy.getNotifications();
        assertEquals(0, notifications.size());
    }
}
