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

import org.chromium.content.R;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the spell check menu.
 */
@RunWith(ContentJUnit4ClassRunner.class)
public class TextSuggestionMenuTest {
    private static final String URL =
            "data:text/html, <div contenteditable id=\"div\" /><span id=\"span\" />";

    @Rule
    public ImeActivityTestRule mRule = new ImeActivityTestRule();

    @Before
    public void setUp() throws Throwable {
        mRule.setUp();
        mRule.fullyLoadUrl(URL);
    }

    @Test
    @LargeTest
    public void testDeleteMarkedWord() throws InterruptedException, Throwable, TimeoutException {
        final ContentViewCore cvc = mRule.getContentViewCore();
        WebContents webContents = cvc.getWebContents();

        DOMUtils.focusNode(webContents, "div");

        SpannableString textToCommit = new SpannableString("hello");
        SuggestionSpan suggestionSpan = new SuggestionSpan(mRule.getContentViewCore().getContext(),
                new String[] {"goodbye"}, SuggestionSpan.FLAG_EASY_CORRECT);
        textToCommit.setSpan(suggestionSpan, 0, 5, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        mRule.commitText(textToCommit, 1);

        DOMUtils.clickNode(cvc, "div");
        waitForMenuToShow(cvc);

        TouchCommon.singleClickView(getDeleteButton(cvc));

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return DOMUtils.getNodeContents(cvc.getWebContents(), "div").equals("");
                } catch (InterruptedException | TimeoutException e) {
                    return false;
                }
            }
        });

        waitForMenuToHide(cvc);
    }

    @Test
    @LargeTest
    public void testApplySuggestion() throws InterruptedException, Throwable, TimeoutException {
        final ContentViewCore cvc = mRule.getContentViewCore();
        WebContents webContents = cvc.getWebContents();

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

        DOMUtils.clickNode(cvc, "span");
        waitForMenuToShow(cvc);

        // There should be 5 child views: 4 suggestions plus the list footer.
        Assert.assertEquals(5, getSuggestionList(cvc).getChildCount());

        Assert.assertEquals(
                "hello suggestion1", ((TextView) getSuggestionButton(cvc, 0)).getText().toString());
        Assert.assertEquals(
                "hello suggestion2", ((TextView) getSuggestionButton(cvc, 1)).getText().toString());
        Assert.assertEquals(
                "suggestion3", ((TextView) getSuggestionButton(cvc, 2)).getText().toString());
        Assert.assertEquals(
                "suggestion4", ((TextView) getSuggestionButton(cvc, 3)).getText().toString());

        TouchCommon.singleClickView(getSuggestionButton(cvc, 2));

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return DOMUtils.getNodeContents(cvc.getWebContents(), "div")
                            .equals("suggestion3");
                } catch (InterruptedException | TimeoutException e) {
                    return false;
                }
            }
        });

        waitForMenuToHide(cvc);
    }

    @Test
    @LargeTest
    public void menuDismissesWhenTappingOutside()
            throws InterruptedException, Throwable, TimeoutException {
        final ContentViewCore cvc = mRule.getContentViewCore();
        WebContents webContents = cvc.getWebContents();

        DOMUtils.focusNode(webContents, "div");

        SpannableString textToCommit = new SpannableString("hello");
        SuggestionSpan suggestionSpan = new SuggestionSpan(mRule.getContentViewCore().getContext(),
                new String[] {"goodbye"}, SuggestionSpan.FLAG_EASY_CORRECT);
        textToCommit.setSpan(suggestionSpan, 0, 5, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        mRule.commitText(textToCommit, 1);

        DOMUtils.clickNode(cvc, "div");
        waitForMenuToShow(cvc);

        // The menu appears below the text, so if we tap on the text again, that
        // should count as tapping outside the menu.
        DOMUtils.clickNode(cvc, "div");
        waitForMenuToHide(cvc);
    }

    private void waitForMenuToShow(ContentViewCore cvc) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                SuggestionsPopupWindow suggestionsPopupWindow =
                        cvc.getTextSuggestionHostForTesting()
                                .getTextSuggestionsPopupWindowForTesting();
                if (suggestionsPopupWindow == null) {
                    return false;
                }

                // On some test runs, even when suggestionsPopupWindow is non-null and
                // suggestionsPopupWindow.isShowing() returns true, the delete button hasn't been
                // measured yet and getWidth()/getHeight() return 0. This causes the menu button
                // click to instead fall on the "Add to dictionary" button. So we have to check that
                // this isn't happening.
                return getDeleteButton(cvc).getWidth() != 0;
            }
        });
    }

    private void waitForMenuToHide(ContentViewCore cvc) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                SuggestionsPopupWindow suggestionsPopupWindow =
                        cvc.getTextSuggestionHostForTesting()
                                .getTextSuggestionsPopupWindowForTesting();
                return suggestionsPopupWindow == null;
            }
        });
    }

    private ListView getSuggestionList(ContentViewCore cvc) {
        View contentView = cvc.getTextSuggestionHostForTesting()
                                   .getTextSuggestionsPopupWindowForTesting()
                                   .getContentViewForTesting();
        return (ListView) contentView.findViewById(R.id.suggestionContainer);
    }

    private View getSuggestionButton(ContentViewCore cvc, int suggestionIndex) {
        return getSuggestionList(cvc).getChildAt(suggestionIndex);
    }

    private View getDeleteButton(ContentViewCore cvc) {
        View contentView = cvc.getTextSuggestionHostForTesting()
                                   .getTextSuggestionsPopupWindowForTesting()
                                   .getContentViewForTesting();
        return contentView.findViewById(R.id.deleteButton);
    }
}
