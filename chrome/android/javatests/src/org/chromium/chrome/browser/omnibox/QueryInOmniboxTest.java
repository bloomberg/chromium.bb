// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.chrome.test.util.OmniboxTestUtils.buildSuggestionMap;

import android.os.Environment;
import android.os.SystemClock;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.Selection;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.widget.ImageButton;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.OmniboxResultsAdapter.OmniboxResultItem;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionsResult;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionsResultBuilder;
import org.chromium.chrome.test.util.OmniboxTestUtils.TestAutocompleteController;
import org.chromium.chrome.test.util.OmniboxTestUtils.TestSuggestionResultsBuilder;
import org.chromium.content.browser.test.util.KeyUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests for Query in Omnibox.
 */
public class QueryInOmniboxTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private static final String QUERY_EXTRACTION_PARAM = "espv=1";
    private static final String SEARCH_TERM = "Puppies";

    private ImageButton mDeleteButton;
    private LocationBarLayout mLocationBar;
    private EmbeddedTestServer mTestServer;
    private TestToolbarDataProvider mTestToolbarDataProvider;
    private UrlBar mUrlBar;

    public QueryInOmniboxTest() {
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

    /**
     * Loading a google https url which is searching for queryText.
     * @throws InterruptedException
     */
    private void loadUrlFromQueryText()
            throws InterruptedException {
        String url = mTestServer.getURL("/chrome/test/data/android/google.html?q="
                + SEARCH_TERM + "&hl=en&client=chrome-mobile&"
                + QUERY_EXTRACTION_PARAM + "&tbm=isch");
        loadUrl(url);
    }

    /**
     * @return The current UrlBar querytext.
     */
    private String getUrlBarQueryText() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<String>() {
            @Override
            public String call() throws Exception {
                return TextUtils.isEmpty(mUrlBar.getQueryText()) ? "" :
                        mUrlBar.getQueryText();
            }
        });
    }

    /**
     * @return The current UrlBar text.
     */
    private CharSequence getUrlBarText() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<CharSequence>() {
            @Override
            public CharSequence call() throws Exception {
                return mUrlBar.getText();
            }
        });
    }

    /**
     * Assert the urlbar is displaying the correct text
     */
    private void checkUrlBarText() {
        assertEquals(SEARCH_TERM, getUrlBarQueryText());
    }

    @SmallTest
    @Feature({"QueryinOmnibox", "Omnibox"})
    public void testQueryInOmnibox() throws InterruptedException {
        loadUrlFromQueryText();
        checkUrlBarText();
    }

    @SmallTest
    @Feature({"QueryinOmnibox", "Omnibox"})
    public void testQueryInOmniboxOnFocus() throws InterruptedException {
        loadUrlFromQueryText();
        OmniboxTestUtils.toggleUrlBarFocus(mUrlBar, true);
        checkUrlBarText();
    }

    @SmallTest
    @Feature({"QueryinOmnibox", "Omnibox"})
    public void testRefocusWithQueryInOmnibox() throws InterruptedException {
        loadUrlFromQueryText();
        assertEquals(SEARCH_TERM, getUrlBarQueryText());
        assertFalse(OmniboxTestUtils.doesUrlBarHaveFocus(mUrlBar));
        OmniboxTestUtils.checkUrlBarRefocus(mUrlBar, 5);
    }

    @MediumTest
    @Feature({"QueryinOmnibox", "Omnibox"})
    public void testDeleteButton() throws InterruptedException {
        loadUrlFromQueryText();
        OmniboxTestUtils.toggleUrlBarFocus(mUrlBar, true);
        checkUrlBarText();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mDeleteButton.callOnClick();
            }
        });
        assertTrue(getUrlBarQueryText().isEmpty());
    }

    @MediumTest
    @Feature({"QueryinOmnibox", "Omnibox"})
    public void testBackspaceDelete() throws InterruptedException {
        loadUrlFromQueryText();
        OmniboxTestUtils.toggleUrlBarFocus(mUrlBar, true);
        checkUrlBarText();
        // Backspace delete the entire querytext
        for (int i = 0; i < SEARCH_TERM.length(); i++) {
            final View v = mUrlBar;
            KeyUtils.singleKeyEventView(getInstrumentation(), v, KeyEvent.KEYCODE_DEL);
        }
        // Query text should be gone
        assertTrue(getUrlBarQueryText().isEmpty());
    }

    @MediumTest
    @Feature({"QueryinOmnibox", "Omnibox"})
    public void testShrinkingAutocompleteTextResults()
            throws InterruptedException {
        loadUrlFromQueryText();
        OmniboxTestUtils.toggleUrlBarFocus(mUrlBar, true);
        final String term = SEARCH_TERM + "i";
        setupAutocompleteSuggestions(term);
        CharSequence urlText = getUrlBarText();
        assertEquals("URL Bar text not autocompleted as expected.",
                SEARCH_TERM + "ingz", getUrlBarQueryText());
        assertEquals(8, Selection.getSelectionStart(urlText));
        assertEquals(11, Selection.getSelectionEnd(urlText));
    }

    @MediumTest
    @Feature({"QueryinOmnibox", "Omnibox"})
    public void testRefineSuggestion() throws InterruptedException {
        loadUrlFromQueryText();
        OmniboxTestUtils.toggleUrlBarFocus(mUrlBar, true);
        final String term = SEARCH_TERM + "i";
        setupAutocompleteSuggestions(term);
        assertEquals(SEARCH_TERM + "ingz", getUrlBarQueryText());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                OmniboxSuggestion refineView =
                        ((OmniboxResultItem) mLocationBar.getSuggestionList()
                                .getAdapter().getItem(1)).getSuggestion();
                ((OmniboxResultsAdapter) mLocationBar.getSuggestionList()
                        .getAdapter()).getSuggestionDelegate()
                        .onRefineSuggestion(refineView);
            }
        });
        CharSequence urlText = getUrlBarText();
        assertEquals("Refine suggestion did not work as expected.",
                SEARCH_TERM + "iblarg", getUrlBarQueryText());
        assertEquals(urlText.length(), Selection.getSelectionStart(urlText));
        assertEquals(urlText.length(), Selection.getSelectionEnd(urlText));
    }

    /**
     * Setup Autocomplete suggestion map, add suggestionListener and autocomplete controller
     * to the location bar.
     * @param term The search term.
     * @throws InterruptedException
     */
    private void setupAutocompleteSuggestions(final String term) throws InterruptedException {
        final String suggestionFor = term.toLowerCase(Locale.US);
        Map<String, List<SuggestionsResult>> suggestionsMap = buildSuggestionMap(
                new TestSuggestionResultsBuilder()
                        .setTextShownFor(suggestionFor)
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        term , null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        term + "ng", null)
                                .setAutocompleteText("ng is awesome"))
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        term, null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        term + "z", null)
                                .setAutocompleteText("ng is hard"))
                        .addSuggestions(new SuggestionsResultBuilder()
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        term, null)
                                .addGeneratedSuggestion(OmniboxSuggestionType.SEARCH_HISTORY,
                                        term + "blarg", null)
                                .setAutocompleteText("ngz")));

        final Object suggestionsProcessedSignal = new Object();
        final AtomicInteger suggestionsLeft = new AtomicInteger(
                suggestionsMap.get(suggestionFor).size());
        OnSuggestionsReceivedListener suggestionsListener = new OnSuggestionsReceivedListener() {
            @Override
            public void onSuggestionsReceived(
                    List<OmniboxSuggestion> suggestions, String inlineAutocompleteText) {
                mLocationBar.onSuggestionsReceived(suggestions, inlineAutocompleteText);
                synchronized (suggestionsProcessedSignal) {
                    int remaining = suggestionsLeft.decrementAndGet();
                    if (remaining == 0) {
                        suggestionsProcessedSignal.notifyAll();
                    } else if (remaining < 0) {
                        fail("Unexpected suggestions received");
                    }
                }
            }
        };
        final TestAutocompleteController controller = new TestAutocompleteController(
                mLocationBar, suggestionsListener, suggestionsMap);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mLocationBar.setAutocompleteController(controller);
                mUrlBar.append("i");
            }
        });
        synchronized (suggestionsProcessedSignal) {
            long endTime = SystemClock.uptimeMillis() + 3000;
            while (suggestionsLeft.get() != 0) {
                long waitTime = endTime - SystemClock.uptimeMillis();
                if (waitTime <= 0) break;
                suggestionsProcessedSignal.wait(waitTime);
            }
        }
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
        mUrlBar = (UrlBar) getActivity().findViewById(R.id.url_bar);
        mLocationBar = (LocationBarLayout) getActivity().findViewById(R.id.location_bar);
        mTestToolbarDataProvider =
                new TestToolbarDataProvider(mLocationBar.getToolbarDataProvider());
        mLocationBar.setToolbarDataProvider(mTestToolbarDataProvider);
        mDeleteButton = (ImageButton) getActivity().findViewById(R.id.delete_button);
    }

    private static class TestToolbarDataProvider implements ToolbarDataProvider {
        private final ToolbarDataProvider mBaseProvider;

        public TestToolbarDataProvider(ToolbarDataProvider baseProvider) {
            mBaseProvider = baseProvider;
        }

        @Override
        public Tab getTab() {
            return mBaseProvider.getTab();
        }

        @Override
        public NewTabPage getNewTabPageForCurrentTab() {
            return mBaseProvider.getNewTabPageForCurrentTab();
        }

        @Override
        public boolean isIncognito() {
            return mBaseProvider.isIncognito();
        }

        @Override
        public String getText() {
            return SEARCH_TERM;
        }

        @Override
        public boolean wouldReplaceURL() {
            return true;
        }

        @Override
        public int getPrimaryColor() {
            return mBaseProvider.getPrimaryColor();
        }

        @Override
        public boolean isUsingBrandColor() {
            return mBaseProvider.isUsingBrandColor();
        }

        @Override
        public String getCorpusChipText() {
            return null;
        }
    }
}
