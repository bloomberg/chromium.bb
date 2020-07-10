// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.settings.privacy.BrowsingDataBridge;
import org.chromium.chrome.browser.settings.privacy.BrowsingDataBridge.OnClearBrowsingDataListener;
import org.chromium.chrome.browser.webapps.TestFetchStorageCallback;
import org.chromium.chrome.browser.webapps.WebappInfo;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.webapps.WebappTestHelper;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;

/**
 * Integration tests for the native BrowsingDataRemover.
 *
 * BrowsingDataRemover is used to delete data from various data storage backends. However, for
 * those backends that live in the Java code, it is not possible to test whether deletions were
 * successful in its own unit tests. This test can do so.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BrowsingDataRemoverIntegrationTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private boolean mCallbackCalled;

    private class CallbackCriteria extends Criteria {
        public CallbackCriteria() {
            mCallbackCalled = false;
        }

        @Override
        public boolean isSatisfied() {
            if (mCallbackCalled) {
                mCallbackCalled = false;
                return true;
            }
            return false;
        }
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private void registerWebapp(final String webappId, final String webappUrl) throws Exception {
        WebappInfo webappInfo = WebappTestHelper.createWebappInfo(webappId, webappUrl);
        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register(webappInfo.id(), callback);
        callback.waitForCallback(0);
        callback.getStorage().updateFromWebappInfo(webappInfo);
    }

    /**
     * Tests that web apps are unregistered after clearing with the "cookies and site data" option.
     * TODO(msramek): Expose more granular datatypes to the Java code, so we can directly test
     * BrowsingDataRemover::RemoveDataMask::REMOVE_WEBAPP_DATA instead of BrowsingDataType.COOKIES.
     */
    @Test
    @MediumTest
    @RetryOnFailure
    public void testUnregisteringWebapps() throws Exception {
        // Register three web apps.
        final HashMap<String, String> apps = new HashMap<String, String>();
        apps.put("webapp1", "https://www.google.com/index.html");
        apps.put("webapp2", "https://www.chrome.com/foo/bar");
        apps.put("webapp3", "http://example.com/");

        for (final Map.Entry<String, String> app : apps.entrySet()) {
            registerWebapp(app.getKey(), app.getValue());
        }
        Assert.assertEquals(apps.keySet(), WebappRegistry.getRegisteredWebappIdsForTesting());

        // Clear cookies and site data excluding the registrable domain "google.com".
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingDataExcludingDomains(
                    new OnClearBrowsingDataListener() {
                        @Override
                        public void onBrowsingDataCleared() {
                            mCallbackCalled = true;
                        }
                    },
                    new int[] {BrowsingDataType.COOKIES}, TimePeriod.ALL_TIME,
                    new String[] {"google.com"}, new int[] {1}, new String[0], new int[0]);
        });
        CriteriaHelper.pollUiThread(new CallbackCriteria());

        // The last two webapps should have been unregistered.
        Assert.assertEquals(new HashSet<String>(Arrays.asList("webapp1")),
                WebappRegistry.getRegisteredWebappIdsForTesting());

        // Clear cookies and site data with no url filter.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(new OnClearBrowsingDataListener() {
                @Override
                public void onBrowsingDataCleared() {
                    mCallbackCalled = true;
                }
            }, new int[] {BrowsingDataType.COOKIES}, TimePeriod.ALL_TIME);
        });
        CriteriaHelper.pollUiThread(new CallbackCriteria());

        // All webapps should have been unregistered.
        Assert.assertTrue(WebappRegistry.getRegisteredWebappIdsForTesting().isEmpty());
    }
}
