// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.input;

import android.support.test.filters.LargeTest;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.R;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.input.SuggestionsPopupWindow;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.JavaScriptUtils;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the spell check menu.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class SpellCheckMenuTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String URL =
            "data:text/html, <div contenteditable id=\"div\">iuvwneaoanls</div>";

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @LargeTest
    @RetryOnFailure
    public void testDeleteMisspelledWord() throws InterruptedException, TimeoutException {
        mActivityTestRule.loadUrl(URL);
        final ContentViewCore cvc =
                mActivityTestRule.getActivity().getActivityTab().getContentViewCore();
        WebContents webContents = cvc.getWebContents();

        // The spell checker is called asynchronously, in an idle-time callback. By waiting for an
        // idle-time callback of our own to be called, we can hopefully ensure that the spell
        // checker has been run before we try to tap on the misspelled word.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents, "window.requestIdleTimeCallback(() -> {});");

        DOMUtils.focusNode(cvc.getWebContents(), "div");
        DOMUtils.clickNode(cvc, "div");

        // Wait for the suggestion menu to show
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                SuggestionsPopupWindow suggestionsPopupWindow =
                        cvc.getTextSuggestionHostForTesting().getSpellCheckPopupWindowForTesting();
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

        TouchCommon.singleClickView(getDeleteButton(cvc));

        CriteriaHelper.pollInstrumentationThread(Criteria.equals("", new Callable<String>() {
            @Override
            public String call() {
                try {
                    return DOMUtils.getNodeContents(cvc.getWebContents(), "div");
                } catch (InterruptedException | TimeoutException e) {
                    return null;
                }
            }
        }));
    }

    private View getDeleteButton(ContentViewCore cvc) {
        View contentView = cvc.getTextSuggestionHostForTesting()
                                   .getSpellCheckPopupWindowForTesting()
                                   .getContentViewForTesting();
        return contentView.findViewById(R.id.deleteButton);
    }
}
