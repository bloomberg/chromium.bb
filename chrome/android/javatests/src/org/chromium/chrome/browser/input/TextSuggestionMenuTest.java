// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.input;

import android.view.View;

import org.junit.Assert;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.R;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TouchCommon;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the text suggestion menu.
 */
public class TextSuggestionMenuTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String URL =
            "data:text/html, <div contenteditable id=\"div\">iuvwneaoanls</div>";

    public TextSuggestionMenuTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    // @LargeTest
    @DisabledTest // TODO(crbug.com/749138): This test is very flaky.
    public void testDeleteMisspelledWord() throws InterruptedException, TimeoutException {
        loadUrl(URL);
        final ContentViewCore cvc = getActivity().getActivityTab().getContentViewCore();
        DOMUtils.focusNode(cvc.getWebContents(), "div");
        DOMUtils.clickNode(cvc, "div");

        // Wait for the suggestion menu to show
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return cvc.getTextSuggestionHostForTesting().getSuggestionsPopupWindowForTesting()
                        != null;
            }
        });

        View view = cvc.getTextSuggestionHostForTesting()
                            .getSuggestionsPopupWindowForTesting()
                            .getContentViewForTesting();
        TouchCommon.singleClickView(view.findViewById(R.id.deleteButton));
        Assert.assertEquals("", DOMUtils.getNodeContents(cvc.getWebContents(), "div"));
    }
}
