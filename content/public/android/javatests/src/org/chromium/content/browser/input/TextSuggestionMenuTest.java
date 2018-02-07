// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.support.test.filters.LargeTest;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.SuggestionSpan;
import android.view.View;
import android.widget.ListView;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.content.R;
import org.chromium.content.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the text suggestion menu.
 */
@RunWith(ContentJUnit4ClassRunner.class)
@CommandLineFlags.Add({"expose-internals-for-testing"})
public class TextSuggestionMenuTest {
    private static final String URL =
            "data:text/html, <div contenteditable id=\"div\" /><span id=\"span\" />";

    @Rule
    public ImeActivityTestRule mRule = new ImeActivityTestRule();

    @Before
    public void setUp() throws Throwable {
        mRule.setUpForUrl(ImeActivityTestRule.INPUT_FORM_HTML);
        mRule.fullyLoadUrl(URL);
    }

    @Test
    @LargeTest
    public void testDeleteWordMarkedWithSuggestionMarker()
            throws InterruptedException, Throwable, TimeoutException {
        final ContentViewCore cvc = mRule.getContentViewCore();
        WebContents webContents = mRule.getWebContents();

        DOMUtils.focusNode(webContents, "div");

        SpannableString textToCommit = new SpannableString("hello");
        SuggestionSpan suggestionSpan = new SuggestionSpan(mRule.getContentViewCore().getContext(),
                new String[] {"goodbye"}, SuggestionSpan.FLAG_EASY_CORRECT);
        textToCommit.setSpan(suggestionSpan, 0, 5, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        mRule.commitText(textToCommit, 1);

        DOMUtils.clickNode(cvc, "div");
        waitForMenuToShow(webContents);

        TouchCommon.singleClickView(getDeleteButton(webContents));

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return DOMUtils.getNodeContents(webContents, "div").equals("");
                } catch (InterruptedException | TimeoutException e) {
                    return false;
                }
            }
        });

        waitForMenuToHide(webContents);
    }

    @Test
    @LargeTest
    public void testDeleteWordMarkedWithSpellingMarker()
            throws InterruptedException, Throwable, TimeoutException {
        final ContentViewCore cvc = mRule.getContentViewCore();
        WebContents webContents = mRule.getWebContents();

        DOMUtils.focusNode(webContents, "div");

        SpannableString textToCommit = new SpannableString("hello");
        mRule.commitText(textToCommit, 1);

        // Wait for renderer to acknowledge commitText(). ImeActivityTestRule.commitText() blocks
        // and waits for the IME thread to finish, but the communication between the IME thread and
        // the renderer is asynchronous, so if we try to run JavaScript right away, the text won't
        // necessarily have been committed yet.
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return DOMUtils.getNodeContents(webContents, "div").equals("hello");
                } catch (InterruptedException | TimeoutException e) {
                    return false;
                }
            }
        });

        // Add a spelling marker on "hello".
        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                "const div = document.getElementById('div');"
                        + "const text = div.firstChild;"
                        + "const range = document.createRange();"
                        + "range.setStart(text, 0);"
                        + "range.setEnd(text, 5);"
                        + "internals.setMarker(document, range, 'spelling');");

        DOMUtils.clickNode(cvc, "div");
        waitForMenuToShow(webContents);

        TouchCommon.singleClickView(getDeleteButton(webContents));

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return DOMUtils.getNodeContents(mRule.getWebContents(), "div").equals("");
                } catch (InterruptedException | TimeoutException e) {
                    return false;
                }
            }
        });

        waitForMenuToHide(webContents);
    }

    @Test
    @LargeTest
    public void testApplySuggestion() throws InterruptedException, Throwable, TimeoutException {
        final ContentViewCore cvc = mRule.getContentViewCore();
        WebContents webContents = mRule.getWebContents();

        DOMUtils.focusNode(webContents, "div");

        // We have a string of length 11 and we set three SuggestionSpans on it
        // to test that the menu shows the right suggestions in the right order:
        //
        // - One span on the word "hello"
        // - One span on the whole string "hello world"
        // - One span on the word "world"
        //
        // We simulate a tap at the end of the string. We should get the
        // suggestions from "world", then the suggestions from "hello world",
        // and not get any suggestions from "hello".

        SpannableString textToCommit = new SpannableString("hello world");

        SuggestionSpan suggestionSpan1 = new SuggestionSpan(mRule.getContentViewCore().getContext(),
                new String[] {"invalid_suggestion"}, SuggestionSpan.FLAG_EASY_CORRECT);
        textToCommit.setSpan(suggestionSpan1, 0, 5, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        SuggestionSpan suggestionSpan2 = new SuggestionSpan(mRule.getContentViewCore().getContext(),
                new String[] {"suggestion3", "suggestion4"}, SuggestionSpan.FLAG_EASY_CORRECT);
        textToCommit.setSpan(suggestionSpan2, 0, 11, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        SuggestionSpan suggestionSpan3 = new SuggestionSpan(mRule.getContentViewCore().getContext(),
                new String[] {"suggestion1", "suggestion2"}, SuggestionSpan.FLAG_EASY_CORRECT);
        textToCommit.setSpan(suggestionSpan3, 6, 11, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        mRule.commitText(textToCommit, 1);

        // Wait for renderer to acknowledge commitText().
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return DOMUtils.getNodeContents(webContents, "div").equals("hello world");
                } catch (InterruptedException | TimeoutException e) {
                    return false;
                }
            }
        });

        DOMUtils.clickNode(cvc, "span");
        waitForMenuToShow(webContents);

        // There should be 5 child views: 4 suggestions plus the list footer.
        Assert.assertEquals(5, getSuggestionList(webContents).getChildCount());

        Assert.assertEquals("hello suggestion1",
                ((TextView) getSuggestionButton(webContents, 0)).getText().toString());
        Assert.assertEquals("hello suggestion2",
                ((TextView) getSuggestionButton(webContents, 1)).getText().toString());
        Assert.assertEquals("suggestion3",
                ((TextView) getSuggestionButton(webContents, 2)).getText().toString());
        Assert.assertEquals("suggestion4",
                ((TextView) getSuggestionButton(webContents, 3)).getText().toString());

        TouchCommon.singleClickView(getSuggestionButton(webContents, 2));

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return DOMUtils.getNodeContents(mRule.getWebContents(), "div")
                            .equals("suggestion3");
                } catch (InterruptedException | TimeoutException e) {
                    return false;
                }
            }
        });

        waitForMenuToHide(webContents);
    }

    @Test
    @LargeTest
    public void testApplyMisspellingSuggestion()
            throws InterruptedException, Throwable, TimeoutException {
        final ContentViewCore cvc = mRule.getContentViewCore();
        WebContents webContents = mRule.getWebContents();

        DOMUtils.focusNode(webContents, "div");

        SpannableString textToCommit = new SpannableString("word");

        SuggestionSpan suggestionSpan = new SuggestionSpan(mRule.getContentViewCore().getContext(),
                new String[] {"replacement"},
                SuggestionSpan.FLAG_EASY_CORRECT | SuggestionSpan.FLAG_MISSPELLED);
        textToCommit.setSpan(suggestionSpan, 0, 4, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

        mRule.commitText(textToCommit, 1);

        DOMUtils.clickNode(cvc, "span");
        waitForMenuToShow(webContents);

        // There should be 2 child views: 1 suggestion plus the list footer.
        Assert.assertEquals(2, getSuggestionList(webContents).getChildCount());

        Assert.assertEquals("replacement",
                ((TextView) getSuggestionButton(webContents, 0)).getText().toString());

        TouchCommon.singleClickView(getSuggestionButton(webContents, 0));

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return DOMUtils.getNodeContents(mRule.getWebContents(), "div")
                            .equals("replacement");
                } catch (InterruptedException | TimeoutException e) {
                    return false;
                }
            }
        });

        waitForMenuToHide(webContents);

        // Verify that the suggestion marker was replaced.
        Assert.assertEquals("0",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                        "internals.markerCountForNode("
                                + "  document.getElementById('div').firstChild, 'suggestion')"));
    }

    @Test
    @LargeTest
    public void suggestionMenuDismissal() throws InterruptedException, Throwable, TimeoutException {
        final ContentViewCore cvc = mRule.getContentViewCore();
        WebContents webContents = mRule.getWebContents();

        DOMUtils.focusNode(webContents, "div");

        SpannableString textToCommit = new SpannableString("hello");
        SuggestionSpan suggestionSpan = new SuggestionSpan(mRule.getContentViewCore().getContext(),
                new String[] {"goodbye"}, SuggestionSpan.FLAG_EASY_CORRECT);
        textToCommit.setSpan(suggestionSpan, 0, 5, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        mRule.commitText(textToCommit, 1);

        DOMUtils.clickNode(cvc, "div");
        waitForMenuToShow(webContents);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TextSuggestionHost.fromWebContents(webContents)
                        .getTextSuggestionsPopupWindowForTesting()
                        .dismiss();
            }
        });
        waitForMenuToHide(webContents);
    }

    private void waitForMenuToShow(WebContents webContents) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                View deleteButton = getDeleteButton(webContents);
                if (deleteButton == null) {
                    return false;
                }

                // suggestionsPopupWindow.isShowing() returns true, the delete button hasn't been
                // measured yet and getWidth()/getHeight() return 0. This causes the menu button
                // click to instead fall on the "Add to dictionary" button. So we have to check that
                // this isn't happening.
                return deleteButton.getWidth() != 0;
            }
        });
    }

    private void waitForMenuToHide(WebContents webContents) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                SuggestionsPopupWindow suggestionsPopupWindow =
                        TextSuggestionHost.fromWebContents(webContents)
                                .getTextSuggestionsPopupWindowForTesting();

                SuggestionsPopupWindow spellCheckPopupWindow =
                        TextSuggestionHost.fromWebContents(webContents)
                                .getSpellCheckPopupWindowForTesting();

                return suggestionsPopupWindow == null && spellCheckPopupWindow == null;
            }
        });
    }

    private View getContentView(WebContents webContents) {
        SuggestionsPopupWindow suggestionsPopupWindow =
                TextSuggestionHost.fromWebContents(webContents)
                        .getTextSuggestionsPopupWindowForTesting();

        if (suggestionsPopupWindow != null) {
            return suggestionsPopupWindow.getContentViewForTesting();
        }

        SuggestionsPopupWindow spellCheckPopupWindow =
                TextSuggestionHost.fromWebContents(webContents)
                        .getSpellCheckPopupWindowForTesting();

        if (spellCheckPopupWindow != null) {
            return spellCheckPopupWindow.getContentViewForTesting();
        }

        return null;
    }

    private ListView getSuggestionList(WebContents webContents) {
        View contentView = getContentView(webContents);
        return (ListView) contentView.findViewById(R.id.suggestionContainer);
    }

    private View getSuggestionButton(WebContents webContents, int suggestionIndex) {
        return getSuggestionList(webContents).getChildAt(suggestionIndex);
    }

    private View getDeleteButton(WebContents webContents) {
        View contentView = getContentView(webContents);
        if (contentView == null) {
            return null;
        }

        return contentView.findViewById(R.id.deleteButton);
    }
}
