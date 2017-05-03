// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.preference.PreferenceScreen;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.util.JsonReader;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.preferences.ButtonPreference;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.privacy.ClearBrowsingDataPreferences;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.IOException;
import java.io.StringReader;
import java.util.Vector;
import java.util.concurrent.TimeoutException;

/**
 * UI Tests for the history page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({"disable-features=AndroidHistoryManager",
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG})
public class HistoryUITest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String HISTORY_URL = "chrome://history-frame/";

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    private static class HistoryItem {
        public final String url;
        public final String title;

        HistoryItem(String url, String title) {
            this.url = url;
            this.title = title;
        }
    }

    private HistoryItem[] getHistoryContents() throws InterruptedException, TimeoutException {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        String jsResults = mActivityTestRule.runJavaScriptCodeInCurrentTab(
                new StringBuilder()
                        .append("var rawResults = document.querySelectorAll('.title');\n")
                        .append("var results = [];\n")
                        .append("for (i = 0; i < rawResults.length; ++i) {\n")
                        .append("    results.push([rawResults[i].childNodes[0].href,\n")
                        .append("    rawResults[i].childNodes[0].childNodes[0].textContent]);\n")
                        .append("}\n")
                        .append("results")
                        .toString());

        JsonReader jsonReader = new JsonReader(new StringReader(jsResults));
        Vector<HistoryItem> results = new Vector<HistoryItem>();
        try {
            jsonReader.beginArray();
            while (jsonReader.hasNext()) {
                jsonReader.beginArray();
                Assert.assertTrue(jsonReader.hasNext());
                String url = jsonReader.nextString();
                Assert.assertTrue(jsonReader.hasNext());
                String title = jsonReader.nextString();
                Assert.assertFalse(jsonReader.hasNext());
                jsonReader.endArray();
                results.add(new HistoryItem(url, title));
            }
            jsonReader.endArray();
            jsonReader.close();
        } catch (IOException ioe) {
            Assert.fail("Failed to evaluate JavaScript: " + jsResults + "\n" + ioe);
        }

        HistoryItem[] history = new HistoryItem[results.size()];
        results.toArray(history);
        return history;
    }

    private void removeSelectedHistoryEntryAtIndex(int index)
            throws InterruptedException, TimeoutException {
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "document.getElementsByClassName('remove-entry')[" + index + "].click();");
    }

    private int getHistoryLength(ContentViewCore cvc)
            throws InterruptedException, TimeoutException {
        String numResultsString = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                cvc.getWebContents(), "document.querySelectorAll('.entry').length");
        int numResults = Integer.parseInt(numResultsString);
        return numResults;
    }

    /**
     * Wait for the UI to show the expected number of results.
     * @param expected The number of results that should be loaded.
     */
    private void waitForResultCount(final ContentViewCore cvc, final int expected) {
        CriteriaHelper.pollInstrumentationThread(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        try {
                            return expected == getHistoryLength(cvc);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                            return false;
                        } catch (TimeoutException e) {
                            e.printStackTrace();
                            return false;
                        }
                    }
                });
    }

    @Test
    @MediumTest
    @Feature({"History"})
    @RetryOnFailure
    public void testSearchHistory() throws InterruptedException, TimeoutException {
        // Introduce some entries in the history page.
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/about.html"));
        mActivityTestRule.loadUrl(
                mTestServer.getURL("/chrome/test/data/android/get_title_test.html"));
        mActivityTestRule.loadUrl(HISTORY_URL);
        waitForResultCount(mActivityTestRule.getActivity().getCurrentContentViewCore(), 2);

        // Search for one of them.
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final CallbackHelper loadCallback = new CallbackHelper();
        TabObserver observer = new EmptyTabObserver() {
            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                if (tab.getUrl().startsWith(HISTORY_URL)) {
                    loadCallback.notifyCalled();
                }
            }
        };
        tab.addObserver(observer);
        mActivityTestRule.runJavaScriptCodeInCurrentTab("historyView.setSearch('about')");
        loadCallback.waitForCallback(0);
        waitForResultCount(mActivityTestRule.getActivity().getCurrentContentViewCore(), 1);

        // Delete the search term.
        mActivityTestRule.runJavaScriptCodeInCurrentTab("historyView.setSearch('')");
        loadCallback.waitForCallback(1);
        waitForResultCount(mActivityTestRule.getActivity().getCurrentContentViewCore(), 2);
        tab.removeObserver(observer);
    }

    // @LargeTest
    // @Feature({"History"})
    @Test
    @DisabledTest
    public void testRemovingEntries() throws InterruptedException, TimeoutException {
        // Urls will be visited in reverse order to preserve the array ordering
        // in the history results.
        String[] testUrls = new String[] {
                mTestServer.getURL("/chrome/test/data/android/google.html"),
                mTestServer.getURL("/chrome/test/data/android/about.html"),
        };

        String[] testTitles = new String[testUrls.length];
        for (int i = testUrls.length - 1; i >= 0; --i) {
            mActivityTestRule.loadUrl(testUrls[i]);
            testTitles[i] = mActivityTestRule.getActivity().getActivityTab().getTitle();
        }

        // Check that the history page contains the visited pages.
        mActivityTestRule.loadUrl(HISTORY_URL);
        waitForResultCount(mActivityTestRule.getActivity().getCurrentContentViewCore(), 2);

        HistoryItem[] history = getHistoryContents();
        for (int i = 0; i < testUrls.length; ++i) {
            Assert.assertEquals(testUrls[i], history[i].url);
            Assert.assertEquals(testTitles[i], history[i].title);
        }

        // Remove the first entry from history.
        Assert.assertTrue(history.length >= 1);
        removeSelectedHistoryEntryAtIndex(0);
        waitForResultCount(mActivityTestRule.getActivity().getCurrentContentViewCore(), 1);

        // Check that now the first result is the second visited page.
        history = getHistoryContents();
        Assert.assertEquals(testUrls[1], history[0].url);
        Assert.assertEquals(testTitles[1], history[0].title);
    }

    @Test
    @LargeTest
    @Feature({"History"})
    @RetryOnFailure
    public void testClearBrowsingData() throws InterruptedException, TimeoutException {
        // Introduce some entries in the history page.
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/google.html"));
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/about.html"));
        mActivityTestRule.loadUrl(HISTORY_URL);
        waitForResultCount(mActivityTestRule.getActivity().getCurrentContentViewCore(), 2);

        // Trigger cleaning up all the browsing data. JS finishing events will make it synchronous
        // to us.
        final Preferences prefActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), Preferences.class, new Runnable() {
                    @Override
                    public void run() {
                        try {
                            mActivityTestRule.runJavaScriptCodeInCurrentTab(
                                    "openClearBrowsingData()");
                        } catch (InterruptedException e) {
                            Assert.fail("Exception occurred while attempting to open clear browing"
                                    + " data");
                        } catch (TimeoutException e) {
                            Assert.fail("Exception occurred while attempting to open clear browing"
                                    + " data");
                        }
                    }
                });
        Assert.assertNotNull("Could not find the preferences activity", prefActivity);

        final ClearBrowsingDataPreferences clearBrowsingFragment =
                (ClearBrowsingDataPreferences) prefActivity.getFragmentForTest();
        Assert.assertNotNull("Could not find clear browsing data fragment", clearBrowsingFragment);

        final ChromeActivity mainActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity().getClass(), new Runnable() {
                    @Override
                    public void run() {
                        ThreadUtils.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                PreferenceScreen screen =
                                        clearBrowsingFragment.getPreferenceScreen();
                                ButtonPreference clearButton =
                                        (ButtonPreference) screen.findPreference(
                                              ClearBrowsingDataPreferences.PREF_CLEAR_BUTTON);
                                clearButton.getOnPreferenceClickListener().onPreferenceClick(
                                        clearButton);
                            }
                        });
                    }
                });
        Assert.assertNotNull("Main never resumed", mainActivity);
        CriteriaHelper.pollUiThread(new Criteria("Main tab never restored") {
            @Override
            public boolean isSatisfied() {
                return !clearBrowsingFragment.isVisible()
                        && mainActivity.getActivityTab() != null
                        && !mainActivity.getActivityTab().isFrozen();
            }
        });
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mainActivity.getCurrentContentViewCore().getWebContents(), "reloadHistory()");
        waitForResultCount(mActivityTestRule.getActivity().getCurrentContentViewCore(), 0);
    }
}
