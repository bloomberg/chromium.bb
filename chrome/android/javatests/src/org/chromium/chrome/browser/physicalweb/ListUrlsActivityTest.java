// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.app.Instrumentation.ActivityMonitor;
import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestTouchUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for the ListUrlsActivity class.
 */
public class ListUrlsActivityTest extends InstrumentationTestCase {
    static final String URL = "https://example.com/";
    static final String TITLE = "Example Title";
    static final String DESC = "Example Website";

    Context mContext;
    MockPwsClient mMockPwsClient;

    private static class TestContextWrapper extends ContextWrapper {
        public List<Intent> startedIntents;

        TestContextWrapper(Context context) {
            super(context);
            startedIntents = new ArrayList<>();
        }

        @Override
        public void startActivity(Intent intent) {
            startedIntents.add(intent);
            super.startActivity(intent);
        }

        public void waitForStartActivity(long timeout) throws InterruptedException {
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return !startedIntents.isEmpty();
                }
            }, timeout, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getTargetContext();
        // Restore the onboarding state
        ContextUtils.getAppSharedPreferences().edit().putInt("physical_web", 2).apply();
        UrlManager urlManager = UrlManager.getInstance();
        urlManager.clearAllUrls();
        mMockPwsClient = new MockPwsClient();
        urlManager.overridePwsClientForTesting(mMockPwsClient);
        urlManager.overrideNotificationManagerForTesting(new MockNotificationManagerProxy());
    }

    @SmallTest
    public void testTapEntryOpensUrl() throws InterruptedException {
        // Ensure the Physical Web is enabled.
        PrivacyPreferencesManager prefsManager = PrivacyPreferencesManager.getInstance();
        prefsManager.setPhysicalWebEnabled(true);
        assertFalse(prefsManager.isPhysicalWebOnboarding());
        assertTrue(prefsManager.isPhysicalWebEnabled());

        // Add URLs.
        addUrl(URL, TITLE, DESC);

        // Launch the Activity.
        ListUrlsActivity listActivity = launchActivity();
        TestContextWrapper testContextWrapper = new TestContextWrapper(listActivity);
        listActivity.overrideContextForTesting(testContextWrapper);
        getInstrumentation().waitForIdleSync();
        View listView = listActivity.findViewById(R.id.physical_web_urls_list);

        // Read the activity and tap the list entry.
        ArrayList<View> entries = new ArrayList<>();
        listView.findViewsWithText(entries, TITLE, View.FIND_VIEWS_WITH_TEXT);
        assertEquals(1, entries.size());
        View entry = entries.get(0);
        TestTouchUtils.singleClickView(getInstrumentation(), entry);
        testContextWrapper.waitForStartActivity(1000);

        // Test the fired intent.
        assertEquals(1, testContextWrapper.startedIntents.size());
        assertEquals(URL, testContextWrapper.startedIntents.get(0).getDataString());
    }

    @SmallTest
    @RetryOnFailure
    public void testNearestEntryInGroupSelectedNoChange() throws InterruptedException {
        // Ensure the Physical Web is enabled.
        PrivacyPreferencesManager prefsManager = PrivacyPreferencesManager.getInstance();
        prefsManager.setPhysicalWebEnabled(true);
        assertFalse(prefsManager.isPhysicalWebOnboarding());
        assertTrue(prefsManager.isPhysicalWebEnabled());
        String url2 = "https://example.com/otherpage";
        UrlInfo urlInfo1 = new UrlInfo(URL, 2.0, System.currentTimeMillis());
        PwsResult pwsResult1 = new PwsResult(URL, URL, null, TITLE, DESC, null);
        UrlInfo urlInfo2 = new UrlInfo(url2, 3.0, System.currentTimeMillis());
        PwsResult pwsResult2 = new PwsResult(url2, url2, null, TITLE, DESC, null);

        // Add the URLs.
        mMockPwsClient.addPwsResult(pwsResult1);
        mMockPwsClient.addPwsResult(pwsResult2);
        mMockPwsClient.addCombinedPwsResults();
        UrlManager.getInstance().addUrl(urlInfo1);
        UrlManager.getInstance().addUrl(urlInfo2);
        getInstrumentation().waitForIdleSync();

        // Launch the Activity.
        ListUrlsActivity listActivity = launchActivity();
        TestContextWrapper testContextWrapper = new TestContextWrapper(listActivity);
        listActivity.overrideContextForTesting(testContextWrapper);
        getInstrumentation().waitForIdleSync();
        View listView = listActivity.findViewById(R.id.physical_web_urls_list);

        // Read the activity and tap the list entry.
        ArrayList<View> entries = new ArrayList<>();
        listView.findViewsWithText(entries, TITLE, View.FIND_VIEWS_WITH_TEXT);
        assertEquals(1, entries.size());
        View entry = entries.get(0);
        TestTouchUtils.singleClickView(getInstrumentation(), entry);
        testContextWrapper.waitForStartActivity(1000);

        // Test the fired intent to make sure the nearer beacon is selected.
        assertEquals(1, testContextWrapper.startedIntents.size());
        assertEquals(URL, testContextWrapper.startedIntents.get(0).getDataString());
    }

    @SmallTest
    @RetryOnFailure
    public void testNearestEntryInGroupSelectedAfterChange() throws InterruptedException {
        // Ensure the Physical Web is enabled.
        PrivacyPreferencesManager prefsManager = PrivacyPreferencesManager.getInstance();
        prefsManager.setPhysicalWebEnabled(true);
        assertFalse(prefsManager.isPhysicalWebOnboarding());
        assertTrue(prefsManager.isPhysicalWebEnabled());
        String url2 = "https://example.com/otherpage";
        UrlInfo urlInfo1 = new UrlInfo(URL, 2.0, System.currentTimeMillis());
        PwsResult pwsResult1 = new PwsResult(URL, URL, null, TITLE, DESC, null);
        UrlInfo urlInfo2 = new UrlInfo(url2, 3.0, System.currentTimeMillis());
        PwsResult pwsResult2 = new PwsResult(url2, url2, null, TITLE, DESC, null);

        // Add the URLs.
        mMockPwsClient.addPwsResult(pwsResult1);
        mMockPwsClient.addPwsResult(pwsResult2);
        mMockPwsClient.addCombinedPwsResults();
        UrlManager.getInstance().addUrl(urlInfo1);
        UrlManager.getInstance().addUrl(urlInfo2);
        getInstrumentation().waitForIdleSync();

        // Launch the Activity.
        ListUrlsActivity listActivity = launchActivity();
        TestContextWrapper testContextWrapper = new TestContextWrapper(listActivity);
        listActivity.overrideContextForTesting(testContextWrapper);
        getInstrumentation().waitForIdleSync();
        urlInfo2.setDistance(1.0);
        getInstrumentation().waitForIdleSync();
        View listView = listActivity.findViewById(R.id.physical_web_urls_list);

        // Read the activity and tap the list entry.
        ArrayList<View> entries = new ArrayList<>();
        listView.findViewsWithText(entries, TITLE, View.FIND_VIEWS_WITH_TEXT);
        assertEquals(1, entries.size());
        View entry = entries.get(0);
        TestTouchUtils.singleClickView(getInstrumentation(), entry);
        testContextWrapper.waitForStartActivity(1000);

        // Test the fired intent to make sure the nearer beacon is selected.
        assertEquals(1, testContextWrapper.startedIntents.size());
        assertEquals(url2, testContextWrapper.startedIntents.get(0).getDataString());
    }

    @SmallTest
    public void testUrlsListEmptyInOnboarding() {
        // In onboarding, we scan for nearby URLs but do not send them to the resolution service to
        // protect the user's privacy. This test checks that the URL list, which only displays
        // resolved URLs, is empty during onboarding.
        PrivacyPreferencesManager prefsManager = PrivacyPreferencesManager.getInstance();
        assertTrue(prefsManager.isPhysicalWebOnboarding());
        assertFalse(prefsManager.isPhysicalWebEnabled());

        // Add URL.
        addUrl(URL, TITLE, DESC);

        // Launch the Activity.
        ListUrlsActivity listActivity = launchActivity();
        TestContextWrapper testContextWrapper = new TestContextWrapper(listActivity);
        listActivity.overrideContextForTesting(testContextWrapper);
        getInstrumentation().waitForIdleSync();
        View listView = listActivity.findViewById(R.id.physical_web_urls_list);

        // Read the activity and check that there are no matching entries.
        ArrayList<View> entries = new ArrayList<>();
        listView.findViewsWithText(entries, TITLE, View.FIND_VIEWS_WITH_TEXT);
        assertEquals(0, entries.size());
    }

    @SmallTest
    public void testUrlsListEmptyWithPhysicalWebDisabled() {
        // With the Physical Web disabled, the URLs list should always be empty.
        PrivacyPreferencesManager prefsManager = PrivacyPreferencesManager.getInstance();
        prefsManager.setPhysicalWebEnabled(false);
        assertFalse(prefsManager.isPhysicalWebOnboarding());
        assertFalse(prefsManager.isPhysicalWebEnabled());

        // Add URL.
        addUrl(URL, TITLE, DESC);

        // Launch the Activity.
        ListUrlsActivity listActivity = launchActivity();
        TestContextWrapper testContextWrapper = new TestContextWrapper(listActivity);
        listActivity.overrideContextForTesting(testContextWrapper);
        getInstrumentation().waitForIdleSync();
        View listView = listActivity.findViewById(R.id.physical_web_urls_list);

        // Read the activity and check that there are no matching entries.
        ArrayList<View> entries = new ArrayList<>();
        listView.findViewsWithText(entries, TITLE, View.FIND_VIEWS_WITH_TEXT);
        assertEquals(0, entries.size());
    }

    public void addUrl(String url, String title, String desc) {
        ArrayList<PwsResult> results = new ArrayList<>();
        results.add(new PwsResult(url, url, null, title, desc, null));
        mMockPwsClient.addPwsResults(results);
        mMockPwsClient.addPwsResults(results);
        UrlManager.getInstance().addUrl(new UrlInfo(url));
        getInstrumentation().waitForIdleSync();
    }

    public ListUrlsActivity launchActivity() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setComponent(new ComponentName(mContext, ListUrlsActivity.class));
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        ActivityMonitor monitor =
                getInstrumentation().addMonitor(ListUrlsActivity.class.getName(), null, false);
        mContext.startActivity(intent);
        ListUrlsActivity activity = (ListUrlsActivity) monitor.waitForActivity();
        activity.overridePwsClientForTesting(mMockPwsClient);
        return activity;
    }
}
