// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.privacy;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTab;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for browsing data deletion.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BrowsingDataTest {
    private static final String TEST_FILE = "/content/test/data/browsing_data/site_data.html";

    private EmbeddedTestServer mTestServer;
    private String mUrl;

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mUrl = mTestServer.getURL(TEST_FILE);
    }

    private void clearBrowsingData(int dataType, int timePeriod) throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(
                    helper::notifyCalled, new int[] {dataType}, timePeriod);
        });
        helper.waitForCallback(0);
    }

    private int getCookieCount() throws Exception {
        String[] out = {""};
        BrowsingDataCounterBridge[] counter = {null};
        CallbackHelper helper = new CallbackHelper();
        BrowsingDataCounterBridge.BrowsingDataCounterCallback callback = (result) -> {
            if (result.equals("Calculating…")) return;
            out[0] = result;
            helper.notifyCalled();
        };
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            counter[0] = new BrowsingDataCounterBridge(
                    callback, BrowsingDataType.COOKIES, ClearBrowsingDataTab.ADVANCED);
        });
        helper.waitForCallback(0);
        // The counter returns a result like "3 sites" or "None".
        if (out[0].equals("None")) return 0;
        String cookieCount = out[0].replaceAll("[^0-9]", "");
        Assert.assertFalse("Result should contain a number: " + out[0], cookieCount.isEmpty());
        return Integer.parseInt(cookieCount);
    }

    private String runJavascriptAsync(String type) throws Exception {
        return JavaScriptUtils.runJavascriptWithAsyncResult(
                mActivityTestRule.getWebContents(), type);
    }

    /**
     * Test cookies deletion.
     */
    @Test
    @SmallTest
    public void testCookiesDeleted() throws Exception {
        Assert.assertEquals(0, getCookieCount());
        mActivityTestRule.loadUrl(mUrl);
        Assert.assertEquals("false", runJavascriptAsync("hasCookie()"));

        runJavascriptAsync("setCookie()");
        Assert.assertEquals("true", runJavascriptAsync("hasCookie()"));
        Assert.assertEquals(1, getCookieCount());

        clearBrowsingData(BrowsingDataType.COOKIES, TimePeriod.LAST_HOUR);
        Assert.assertEquals("false", runJavascriptAsync("hasCookie()"));
        Assert.assertEquals(0, getCookieCount());
    }

    /**
     * Test site data deletion.
     */
    @Test
    @SmallTest
    public void testSiteDataDeleted() throws Exception {
        // TODO(dullweber): Investigate, why WebSql fails this test.
        List<String> siteData = Arrays.asList("LocalStorage", "ServiceWorker", "CacheStorage",
                "IndexedDb", "FileSystem" /*, "WebSql"*/);
        mActivityTestRule.loadUrl(mUrl);

        for (String type : siteData) {
            Assert.assertEquals(type, 0, getCookieCount());
            Assert.assertEquals(type, "false", runJavascriptAsync("has" + type + "()"));

            runJavascriptAsync("set" + type + "()");
            Assert.assertEquals(type, 1, getCookieCount());
            Assert.assertEquals(type, "true", runJavascriptAsync("has" + type + "()"));

            clearBrowsingData(BrowsingDataType.COOKIES, TimePeriod.LAST_HOUR);
            Assert.assertEquals(type, 0, getCookieCount());
            Assert.assertEquals(type, "false", runJavascriptAsync("has" + type + "()"));

            // Some types create data by checking for them, so we need to do a cleanup at the end.
            clearBrowsingData(BrowsingDataType.COOKIES, TimePeriod.LAST_HOUR);
        }
    }

    /**
     * Test all data deletion for incognito profile. This only checks to see if an android specific
     * code crashes or not. For details see, crbug.com/990624.
     */
    @Test
    @SmallTest
    public void testAllDataDeletedForIncognito() throws Exception {
        // TODO(roagarwal) : Crashes on BrowsingDataType.SITE_SETTINGS, BrowsingDataType.BOOKMARKS
        // data types.
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingDataIncognitoForTesting(
                    helper::notifyCalled,
                    new int[] {BrowsingDataType.HISTORY, BrowsingDataType.CACHE,
                            BrowsingDataType.COOKIES, BrowsingDataType.PASSWORDS,
                            BrowsingDataType.FORM_DATA},
                    TimePeriod.LAST_HOUR);
        });
        helper.waitForCallback(0);
    }

    /**
     * Test history deletion.
     */
    @Test
    @SmallTest
    public void testHistoryDeleted() throws Exception {
        Assert.assertEquals(0, getCookieCount());
        mActivityTestRule.loadUrlInNewTab(mUrl);
        Assert.assertEquals("false", runJavascriptAsync("hasHistory()"));

        runJavascriptAsync("setHistory()");
        Assert.assertEquals("true", runJavascriptAsync("hasHistory()"));

        clearBrowsingData(BrowsingDataType.HISTORY, TimePeriod.LAST_HOUR);
        Assert.assertEquals("false", runJavascriptAsync("hasHistory()"));
    }
}
