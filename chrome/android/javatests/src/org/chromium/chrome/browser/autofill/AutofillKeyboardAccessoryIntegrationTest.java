// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for autofill keyboard accessory.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@EnableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY,
        ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillKeyboardAccessoryIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    private void loadTestPage() throws InterruptedException, ExecutionException, TimeoutException {
        mHelper.loadTestPage("/chrome/test/data/autofill/autofill_test_form.html", false, false);
        ManualFillingTestHelper.createAutofillTestProfiles();
        DOMUtils.waitForNonZeroNodeBounds(mHelper.getWebContents(), "NAME_FIRST");
    }

    /**
     * Autofocused fields should not show a keyboard accessory.
     */
    @Test
    @MediumTest
    public void testAutofocusedFieldDoesNotShowKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage();
        CriteriaHelper.pollUiThread(() -> {
            View accessory = mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory);
            return accessory == null || !accessory.isShown();
        });
    }

    /**
     * Tapping on an input field should show a keyboard and its keyboard accessory.
     */
    @Test
    @MediumTest
    public void testTapInputFieldShowsKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage();
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST");
        mHelper.waitForKeyboardAccessoryToBeShown();
    }

    /**
     * Switching fields should re-scroll the keyboard accessory to the left.
     */
    @Test
    @MediumTest
    public void testSwitchFieldsRescrollsKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage();
        mHelper.clickNodeAndShowKeyboard("EMAIL_ADDRESS");
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Scroll to the second position and check it actually happened.
        ThreadUtils.runOnUiThreadBlocking(() -> getSuggestionsComponent().scrollToPosition(2));
        CriteriaHelper.pollUiThread(() -> {
            return getSuggestionsComponent().computeHorizontalScrollOffset() > 0;
        }, "Should keep the manual scroll position.");

        // Clicking any other node should now scroll the items back to the initial position.
        mHelper.clickNodeAndShowKeyboard("NAME_LAST");
        CriteriaHelper.pollUiThread(() -> {
            return getSuggestionsComponent().computeHorizontalScrollOffset() == 0;
        }, "Should be scrolled back to position 0.");
    }

    /**
     * Selecting a keyboard accessory suggestion should hide the keyboard and its keyboard
     * accessory.
     */
    @Test
    @MediumTest
    public void testSelectSuggestionHidesKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage();
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST");
        mHelper.waitForKeyboardAccessoryToBeShown();

        ThreadUtils.runOnUiThreadBlocking(() -> getFirstSuggestion().performClick());
        mHelper.waitForKeyboardAccessoryToDisappear();
    }

    private RecyclerView getSuggestionsComponent() {
        final ViewGroup keyboardAccessory = ThreadUtils.runOnUiThreadBlockingNoException(
                () -> mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory));
        assert keyboardAccessory != null;
        return (RecyclerView) keyboardAccessory.findViewById(R.id.bar_items_view);
    }

    private View getFirstSuggestion() {
        ViewGroup recyclerView = getSuggestionsComponent();
        assert recyclerView != null;
        return recyclerView.getChildAt(0);
    }
}
