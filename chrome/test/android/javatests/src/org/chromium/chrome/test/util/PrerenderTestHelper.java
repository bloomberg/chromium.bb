// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.util.Pair;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.TabLoadStatus;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.OmniboxResultsAdapter;
import org.chromium.chrome.browser.omnibox.OmniboxResultsAdapter.OmniboxResultItem;
import org.chromium.chrome.browser.omnibox.OmniboxResultsAdapter.OmniboxSuggestionDelegate;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestion;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Utility class for common methods to test prerendering.
 */
public class PrerenderTestHelper {
    private static final int UI_DELAY_MS = 100;
    private static final int WAIT_FOR_RESPONSE_MS = 10000;
    private static final int SHORT_TIMEOUT_MS = 200;
    private static final int MAX_NUM_REPEATS_FOR_TRAINING = 7;

    private static boolean hasTabPrerenderedUrl(final Tab tab, final String url) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return tab.hasPrerenderedUrl(url);
            }
        });
    }

    /**
     * Waits for the specified URL to be prerendered.
     * Returns true if it succeeded, false if it was not prerendered before the timeout.
     * shortTimeout should be set to true when expecting this function to return false, as to
     * make the tests run faster.
     */
    public static boolean waitForPrerenderUrl(final Tab tab, final String url,
            boolean shortTimeout) throws InterruptedException {
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return hasTabPrerenderedUrl(tab, url);
            }
        }, shortTimeout ? SHORT_TIMEOUT_MS : WAIT_FOR_RESPONSE_MS, UI_DELAY_MS);

        return hasTabPrerenderedUrl(tab, url);
    }

    private static Pair<OmniboxSuggestion, Integer> waitForOmniboxSuggestion(
            final LocationBarLayout locationBar, final String url) throws InterruptedException {
        final AtomicReference<Pair<OmniboxSuggestion, Integer>> result =
                new AtomicReference<Pair<OmniboxSuggestion, Integer>>();
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                OmniboxResultsAdapter adapter =
                        (OmniboxResultsAdapter) locationBar.getSuggestionList().getAdapter();
                for (int i = 0; i < adapter.getCount(); i++) {
                    OmniboxResultItem popupItem = (OmniboxResultItem) adapter.getItem(i);
                    OmniboxSuggestion matchedSuggestion = popupItem.getSuggestion();
                    if (matchedSuggestion.getUrl().equals(url)) {
                        result.set(new Pair<OmniboxSuggestion, Integer>(matchedSuggestion, i));
                        return true;
                    }
                }
                return false;
            }
        }, SHORT_TIMEOUT_MS, UI_DELAY_MS);
        return result.get();
    }

    /**
     * Clears the omnibox.
     *
     * @param testBase ChromeTabbedActivityTestBase instance.
     */
    public static void clearOmnibox(ChromeTabbedActivityTestBase testBase)
            throws InterruptedException {
        testBase.typeInOmnibox("", false);
    }

    /**
     * Clears the omnibox and types in the url character-by-character.
     *
     * @param url url to type into the omnibox.
     * @param testBase ChromeTabbedActivityTestBase instance.
     */
    public static void clearOmniboxAndTypeUrl(String url, ChromeTabbedActivityTestBase testBase)
            throws InterruptedException {
        clearOmnibox(testBase);
        testBase.typeInOmnibox(url, true);
    }

    /**
     * Trains the autocomplete action predictor to pre-render a url.
     *
     * @param testUrl Url to prerender
     * @param testBase ChromeTabbedActivityTestBase instance.
     */
    public static void trainAutocompleteActionPredictorAndTestPrerender(final String testUrl,
            ChromeTabbedActivityTestBase testBase)
            throws InterruptedException {
        // TODO(yusufo): Replace with external prerender handler instead of training.
        ChromeTabUtils.newTabFromMenu(testBase.getInstrumentation(), testBase.getActivity());
        for (int i = 0; i < MAX_NUM_REPEATS_FOR_TRAINING; i++) {
            // Navigate to another URL otherwise pre-rendering won't happen.
            // Sometimes calling clearOmnibox immediately after won't actually
            // clear it. This seems to help.
            clearOmnibox(testBase);
            testBase.typeInOmnibox(testUrl, true);
            final LocationBarLayout locationBar =
                    (LocationBarLayout) testBase.getActivity().findViewById(R.id.location_bar);
            if (waitForPrerenderUrl(testBase.getActivity().getActivityTab(), testUrl, true)) {
                break;
            }

            final Pair<OmniboxSuggestion, Integer> suggestionPair =
                    waitForOmniboxSuggestion(locationBar, testUrl);
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    OmniboxResultsAdapter ora =
                            (OmniboxResultsAdapter) locationBar.getSuggestionList().getAdapter();
                    OmniboxSuggestionDelegate suggestionDelegate =
                            ora.getSuggestionDelegate();
                    suggestionDelegate.onSelection(suggestionPair.first, suggestionPair.second);
                }
            });
        }
        ChromeTabUtils.closeCurrentTab(testBase.getInstrumentation(), testBase.getActivity());
        clearOmnibox(testBase);

        testBase.typeInOmnibox(testUrl, true);
        final Tab tab = testBase.getActivity().getActivityTab();
        ChromeTabbedActivityTestBase.assertTrue("URL was not prerendered.",
                PrerenderTestHelper.waitForPrerenderUrl(tab, testUrl, false));
    }

    /**
     * Checks if the load url result is prerendered.
     *
     * @param result Result from a page load.
     * @return Whether the result param indicates a prerendered url.
     */
    public static boolean isLoadUrlResultPrerendered(int result) {
        return result == TabLoadStatus.FULL_PRERENDERED_PAGE_LOAD
                || result == TabLoadStatus.PARTIAL_PRERENDERED_PAGE_LOAD;
    }
}
