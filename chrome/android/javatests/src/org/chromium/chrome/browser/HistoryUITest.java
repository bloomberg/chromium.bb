// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Environment;
import android.preference.PreferenceScreen;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.util.JsonReader;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.ButtonPreference;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.privacy.ClearBrowsingDataPreferences;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.CallbackHelper;
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
public class HistoryUITest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private static final String HISTORY_URL = "chrome://history-frame/";

    private EmbeddedTestServer mTestServer;

    public HistoryUITest() {
        super(ChromeActivity.class);
        mSkipCheckHttpServer = true;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
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
        getInstrumentation().waitForIdleSync();
        String jsResults = runJavaScriptCodeInCurrentTab(new StringBuilder()
                .append("var rawResults = document.querySelectorAll('.title');\n")
                .append("var results = [];\n")
                .append("for (i = 0; i < rawResults.length; ++i) {\n")
                .append("    results.push([rawResults[i].childNodes[0].href,\n")
                .append("    rawResults[i].childNodes[0].childNodes[0].textContent]);\n")
                .append("}\n")
                .append("results").toString());

        JsonReader jsonReader = new JsonReader(new StringReader(jsResults));
        Vector<HistoryItem> results = new Vector<HistoryItem>();
        try {
            jsonReader.beginArray();
            while (jsonReader.hasNext()) {
                jsonReader.beginArray();
                assertTrue(jsonReader.hasNext());
                String url = jsonReader.nextString();
                assertTrue(jsonReader.hasNext());
                String title = jsonReader.nextString();
                assertFalse(jsonReader.hasNext());
                jsonReader.endArray();
                results.add(new HistoryItem(url, title));
            }
            jsonReader.endArray();
            jsonReader.close();
        } catch (IOException ioe) {
            fail("Failed to evaluate JavaScript: " + jsResults + "\n" + ioe);
        }

        HistoryItem[] history = new HistoryItem[results.size()];
        results.toArray(history);
        return history;
    }

    private void removeSelectedHistoryEntryAtIndex(int index)
            throws InterruptedException, TimeoutException {
        runJavaScriptCodeInCurrentTab(
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
     * @throws InterruptedException
     */
    private void waitForResultCount(final ContentViewCore cvc, final int expected)
            throws InterruptedException {
        CriteriaHelper.pollForCriteria(
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

    @MediumTest
    @Feature({"History"})
    public void testSearchHistory() throws InterruptedException, TimeoutException {
        // Introduce some entries in the history page.
        loadUrl(mTestServer.getURL("/chrome/test/data/android/about.html"));
        loadUrl(mTestServer.getURL("/chrome/test/data/android/get_title_test.html"));
        loadUrl(HISTORY_URL);
        waitForResultCount(getActivity().getCurrentContentViewCore(), 2);

        // Search for one of them.
        Tab tab = getActivity().getActivityTab();
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
        runJavaScriptCodeInCurrentTab("historyView.setSearch('about')");
        loadCallback.waitForCallback(0);
        waitForResultCount(getActivity().getCurrentContentViewCore(), 1);

        // Delete the search term.
        runJavaScriptCodeInCurrentTab("historyView.setSearch('')");
        loadCallback.waitForCallback(1);
        waitForResultCount(getActivity().getCurrentContentViewCore(), 2);
        tab.removeObserver(observer);
    }

    @LargeTest
    @Feature({"History"})
    public void testRemovingEntries() throws InterruptedException, TimeoutException {
        // Urls will be visited in reverse order to preserve the array ordering
        // in the history results.
        String[] testUrls = new String[] {
                mTestServer.getURL("/chrome/test/data/android/google.html"),
                mTestServer.getURL("/chrome/test/data/android/about.html"),
        };

        String[] testTitles = new String[testUrls.length];
        for (int i = testUrls.length - 1; i >= 0; --i) {
            loadUrl(testUrls[i]);
            testTitles[i] = getActivity().getActivityTab().getTitle();
        }

        // Check that the history page contains the visited pages.
        loadUrl(HISTORY_URL);
        waitForResultCount(getActivity().getCurrentContentViewCore(), 2);

        HistoryItem[] history = getHistoryContents();
        for (int i = 0; i < testUrls.length; ++i) {
            assertEquals(testUrls[i], history[i].url);
            assertEquals(testTitles[i], history[i].title);
        }

        // Remove the first entry from history.
        assertTrue(history.length >= 1);
        removeSelectedHistoryEntryAtIndex(0);
        waitForResultCount(getActivity().getCurrentContentViewCore(), 1);

        // Check that now the first result is the second visited page.
        history = getHistoryContents();
        assertEquals(testUrls[1], history[0].url);
        assertEquals(testTitles[1], history[0].title);
    }

    @LargeTest
    @Feature({"History"})
    public void testClearBrowsingData() throws InterruptedException, TimeoutException {
        // Introduce some entries in the history page.
        loadUrl(mTestServer.getURL("/chrome/test/data/android/google.html"));
        loadUrl(mTestServer.getURL("/chrome/test/data/android/about.html"));
        loadUrl(HISTORY_URL);
        waitForResultCount(getActivity().getCurrentContentViewCore(), 2);

        // Trigger cleaning up all the browsing data. JS finishing events will make it synchronous
        // to us.
        final Preferences prefActivity = ActivityUtils.waitForActivity(
                getInstrumentation(), Preferences.class, new Runnable() {
                    @Override
                    public void run() {
                        try {
                            runJavaScriptCodeInCurrentTab("openClearBrowsingData()");
                        } catch (InterruptedException e) {
                            fail("Exception occurred while attempting to open clear browing data");
                        } catch (TimeoutException e) {
                            fail("Exception occurred while attempting to open clear browing data");
                        }
                    }
                });
        assertNotNull("Could not find the preferences activity", prefActivity);

        final ClearBrowsingDataPreferences clearBrowsingFragment =
                (ClearBrowsingDataPreferences) prefActivity.getFragmentForTest();
        assertNotNull("Could not find clear browsing data fragment", clearBrowsingFragment);

        final ChromeActivity mainActivity = ActivityUtils.waitForActivity(
                getInstrumentation(), getActivity().getClass(), new Runnable() {
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
        assertNotNull("Main never resumed", mainActivity);
        CriteriaHelper.pollForUIThreadCriteria(new Criteria("Main tab never restored") {
            @Override
            public boolean isSatisfied() {
                return !clearBrowsingFragment.isVisible()
                        && mainActivity.getActivityTab() != null
                        && !mainActivity.getActivityTab().isFrozen();
            }
        });
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mainActivity.getCurrentContentViewCore().getWebContents(), "reloadHistory()");
        waitForResultCount(getActivity().getCurrentContentViewCore(), 0);
    }
}
