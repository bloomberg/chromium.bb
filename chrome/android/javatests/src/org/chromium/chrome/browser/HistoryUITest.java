// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Dialog;
import android.support.v7.app.AlertDialog;
import android.test.FlakyTest;
import android.util.JsonReader;
import android.widget.Button;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.privacy.ClearBrowsingDataDialogFragment;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.content.browser.test.util.UiUtils;

import java.io.IOException;
import java.io.StringReader;
import java.util.Vector;
import java.util.concurrent.TimeoutException;

public class HistoryUITest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private static final String HISTORY_URL = "chrome://history-frame/";

    public HistoryUITest() {
        super(ChromeActivity.class);
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
        } catch (IOException ioe) {
            fail("Failed to evaluate JavaScript: " + jsResults + "\n" + ioe);
        }

        HistoryItem[] history = new HistoryItem[results.size()];
        results.toArray(history);
        return history;
    }

    private void removeSelectedEntries(int[] indices)
            throws InterruptedException, TimeoutException {
        // Build the query with the indices of the history elements to delete.
        StringBuilder jsQuery = new StringBuilder("var toCheck = [");
        for (int i = 0; i < indices.length; ++i) {
            if (i != 0) {
                jsQuery.append(", ");
            }
            jsQuery.append(Integer.toString(indices[i]));
        }

        // Remove the specified indices from history.
        runJavaScriptCodeInCurrentTab(jsQuery.append("];\n")
                .append("var checkboxes = Array.prototype.slice.call(")
                .append("    document.getElementsByTagName('input')).")
                .append("    filter(function(elem, index, array) {")
                .append("    return elem.type == 'checkbox'; })\n")
                .append("for (i = 0; i < checkboxes.length; ++i) {\n")
                .append("    checkboxes[i].checked = false;\n")
                .append("}\n")
                .append("for (i = 0; i < toCheck.length; ++i) {\n")
                .append("    checkboxes[toCheck[i]].checked = true;\n")
                .append("}\n")
                .append("window.confirm = function() { return true; };\n")
                .append("removeItems();\n").toString());

        // The deleteComplete() JavaScript method is called when the operation finishes.
        // Since we cannot synchronously wait for a JS callback, keep checking until the delete
        // queue becomes empty.
        Boolean pending;
        do {
            pending = Boolean.parseBoolean(runJavaScriptCodeInCurrentTab("deleteQueue.length > 0"));
        } while (pending);
    }

    private int getHistoryLength(ContentViewCore cvc)
            throws InterruptedException, TimeoutException {
        getInstrumentation().waitForIdleSync();

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
    private void assertResultCountReaches(final ContentViewCore cvc, final int expected)
            throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(
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
                }));
    }

    /*
     * @MediumTest
     * @Feature({"History"})
     * Bug 5969084
     */
    @DisabledTest
    public void testSearchHistory() throws InterruptedException, TimeoutException {
        // Introduce some entries in the history page.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/about.html"));
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/get_title_test.html"));
        loadUrl(HISTORY_URL);
        UiUtils.settleDownUI(getInstrumentation());
        assertResultCountReaches(getActivity().getCurrentContentViewCore(), 2);

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
        assertResultCountReaches(getActivity().getCurrentContentViewCore(), 1);

        // Delete the search term.
        runJavaScriptCodeInCurrentTab("historyView.setSearch('')");
        loadCallback.waitForCallback(1);
        assertResultCountReaches(getActivity().getCurrentContentViewCore(), 2);
        tab.removeObserver(observer);
    }

    /*
     * @LargeTest
     * @Feature({"History"})
     * Bug 6164471
     */
    @FlakyTest
    public void testRemovingEntries() throws InterruptedException, TimeoutException {
        // Urls will be visited in reverse order to preserve the array ordering
        // in the history results.
        String[] testUrls = new String[] {
                TestHttpServerClient.getUrl("chrome/test/data/android/google.html"),
                TestHttpServerClient.getUrl("chrome/test/data/android/about.html"),
        };

        String[] testTitles = new String[testUrls.length];
        for (int i = testUrls.length - 1; i >= 0; --i) {
            loadUrl(testUrls[i]);
            testTitles[i] = getActivity().getActivityTab().getTitle();
        }

        // Check that the history page contains the visited pages.
        loadUrl(HISTORY_URL);
        UiUtils.settleDownUI(getInstrumentation());
        HistoryItem[] history = getHistoryContents();
        assertEquals(testUrls.length, history.length);
        for (int i = 0; i < testUrls.length; ++i) {
            assertEquals(testUrls[i], history[i].url);
            assertEquals(testTitles[i], history[i].title);
        }

        // Remove the first entry from history.
        assertTrue(history.length >= 1);
        removeSelectedEntries(new int[]{ 0 });

        // Check that now the first result is the second visited page.
        history = getHistoryContents();
        assertEquals(testUrls.length - 1, history.length);
        assertEquals(testUrls[1], history[0].url);
        assertEquals(testTitles[1], history[0].title);
    }

    /*
     * @LargeTest
     * @Feature({"History"})
     * Bug 5971989
     */
    @FlakyTest
    public void testClearBrowsingData() throws InterruptedException, TimeoutException {
        // Introduce some entries in the history page.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/google.html"));
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/about.html"));
        loadUrl(HISTORY_URL);
        UiUtils.settleDownUI(getInstrumentation());
        assertResultCountReaches(getActivity().getCurrentContentViewCore(), 2);

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

        final ClearBrowsingDataDialogFragment clearBrowsingFragment =
                ActivityUtils.waitForFragment(
                        prefActivity, ClearBrowsingDataDialogFragment.FRAGMENT_TAG);
        assertNotNull("Could not find clear browsing data fragment", clearBrowsingFragment);

        Dialog dialog = clearBrowsingFragment.getDialog();
        final Button clearButton = ((AlertDialog) dialog).getButton(
                AlertDialog.BUTTON_POSITIVE);
        assertNotNull("Could not find Clear button.", clearButton);

        TestTouchUtils.performClickOnMainSync(getInstrumentation(), clearButton);
        assertTrue("Clear browsing dialog never hidden",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return !clearBrowsingFragment.isVisible();
                    }
                }));

        final ChromeActivity mainActivity = ActivityUtils.waitForActivity(
                getInstrumentation(), getActivity().getClass(), new Runnable() {
                    @Override
                    public void run() {
                        ThreadUtils.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                prefActivity.finish();
                            }
                        });
                    }
                });
        assertNotNull("Main never resumed", mainActivity);
        assertTrue("Main tab never restored", CriteriaHelper.pollForUIThreadCriteria(
                new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mainActivity.getActivityTab() != null
                                && !mainActivity.getActivityTab().isFrozen();
                    }
                }));
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mainActivity.getCurrentContentViewCore().getWebContents(), "reloadHistory()");
        assertResultCountReaches(getActivity().getCurrentContentViewCore(), 0);
    }
}
