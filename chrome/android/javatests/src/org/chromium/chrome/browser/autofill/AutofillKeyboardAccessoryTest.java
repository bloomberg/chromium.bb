// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests for autofill keyboard accessory.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.
Add({"enable-features=AutofillKeyboardAccessory", ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillKeyboardAccessoryTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private final AtomicReference<WebContents> mWebContentsRef = new AtomicReference<WebContents>();
    private final AtomicReference<ViewGroup> mContainerRef = new AtomicReference<ViewGroup>();
    private final AtomicReference<ViewGroup> mKeyboardAccessoryRef =
            new AtomicReference<ViewGroup>();

    private void loadTestPage(boolean isRtl) throws InterruptedException, ExecutionException,
            TimeoutException {
        mActivityTestRule.startMainActivityWithURL(UrlUtils.encodeHtmlDataUri("<html"
                + (isRtl ? " dir=\"rtl\"" : "") + "><head>"
                + "<meta name=\"viewport\""
                + "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>"
                + "<body><form method=\"POST\">"
                + "<input type=\"text\" id=\"fn\" autocomplete=\"given-name\" autofocus/><br>"
                + "<input type=\"text\" id=\"ln\" autocomplete=\"family-name\" /><br>"
                + "<textarea id=\"sa\" autocomplete=\"street-address\"></textarea><br>"
                + "<input type=\"text\" id=\"a1\" autocomplete=\"address-line1\" /><br>"
                + "<input type=\"text\" id=\"a2\" autocomplete=\"address-line2\" /><br>"
                + "<input type=\"text\" id=\"ct\" autocomplete=\"address-level2\" /><br>"
                + "<input type=\"text\" id=\"zc\" autocomplete=\"postal-code\" /><br>"
                + "<input type=\"text\" id=\"em\" autocomplete=\"email\" /><br>"
                + "<input type=\"text\" id=\"ph\" autocomplete=\"tel\" /><br>"
                + "<input type=\"text\" id=\"fx\" autocomplete=\"fax\" /><br>"
                + "<select id=\"co\" autocomplete=\"country\"><br>"
                + "<option value=\"BR\">Brazil</option>"
                + "<option value=\"US\">United States</option>"
                + "</select>"
                + "<input type=\"submit\" />"
                + "</form></body></html>"));
        new AutofillTestHelper().setProfile(new AutofillProfile("", "https://www.example.com",
                "Johnathan Smithonian-Jackson", "Acme Inc", "1 Main\nApt A", "CA", "San Francisco",
                "", "94102", "", "US", "(415) 888-9999", "john@acme.inc", "en"));
        new AutofillTestHelper().setProfile(new AutofillProfile("", "https://www.example.com",
                "Jane Erika Donovanova", "Acme Inc", "1 Main\nApt A", "CA", "San Francisco", "",
                "94102", "", "US", "(415) 999-0000", "jane@acme.inc", "en"));
        new AutofillTestHelper().setProfile(new AutofillProfile("", "https://www.example.com",
                "Marcus McSpartangregor", "Acme Inc", "1 Main\nApt A", "CA", "San Francisco", "",
                "94102", "", "US", "(415) 999-0000", "marc@acme.inc", "en"));
        setRtlForTesting(isRtl);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            mWebContentsRef.set(tab.getWebContents());
            mContainerRef.set(tab.getContentView());
            mKeyboardAccessoryRef.set(mActivityTestRule.getActivity()
                    .getWindowAndroid()
                    .getKeyboardAccessoryView());
        });
        DOMUtils.waitForNonZeroNodeBounds(mWebContentsRef.get(), "fn");
    }

    /**
     * Autofocused fields should not show a keyboard accessory.
     */
    @Test
    @MediumTest
    @Feature({"keyboard-accessory"})
    public void testAutofocusedFieldDoesNotShowKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage(false);
        Assert.assertTrue("Keyboard accessory should be hidden.",
                ThreadUtils
                        .runOnUiThreadBlocking(
                                () -> mKeyboardAccessoryRef.get().getVisibility() == View.GONE)
                        .booleanValue());
    }

    /**
     * Tapping on an input field should show a keyboard and its keyboard accessory.
     */
    @Test
    @MediumTest
    @Feature({"keyboard-accessory"})
    public void testTapInputFieldShowsKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage(false);
        DOMUtils.clickNode(mWebContentsRef.get(), "fn");
        CriteriaHelper.pollUiThread(new Criteria("Keyboard should be showing.") {
            @Override
            public boolean isSatisfied() {
                return UiUtils.isKeyboardShowing(
                        mActivityTestRule.getActivity(), mContainerRef.get());
            }
        });
        Assert.assertTrue("Keyboard accessory should be showing.",
                ThreadUtils
                        .runOnUiThreadBlocking(
                                () -> mKeyboardAccessoryRef.get().getVisibility() == View.VISIBLE)
                        .booleanValue());
    }

    /**
     * Switching fields should re-scroll the keyboard accessory to the left.
     */
    @Test
    @MediumTest
    @Feature({"keyboard-accessory"})
    public void testSwitchFieldsRescrollsKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage(false);
        DOMUtils.clickNode(mWebContentsRef.get(), "fn");
        CriteriaHelper.pollUiThread(new Criteria("Keyboard should be showing.") {
            @Override
            public boolean isSatisfied() {
                return UiUtils.isKeyboardShowing(
                        mActivityTestRule.getActivity(), mContainerRef.get());
            }
        });
        ThreadUtils.runOnUiThreadBlocking(() -> getSuggestionsComponent().scrollTo(2000, 0));
        CriteriaHelper.pollUiThread(
                new Criteria("First suggestion should be off the screen after manual scroll.") {
                    @Override
                    public boolean isSatisfied() {
                        View suggestion = getSuggestionAt(0);
                        if (suggestion != null) {
                            int[] location = new int[2];
                            suggestion.getLocationOnScreen(location);
                            return location[0] < 0;
                        } else {
                            return false;
                        }
                    }
                });
        DOMUtils.clickNode(mWebContentsRef.get(), "ln");
        CriteriaHelper.pollUiThread(
                new Criteria("First suggestion should be on the screen after switching fields.") {
                    @Override
                    public boolean isSatisfied() {
                        int[] location = new int[2];
                        getSuggestionAt(0).getLocationOnScreen(location);
                        return location[0] > 0;
                    }
                });
    }

    /**
     * Switching fields in RTL should re-scroll the keyboard accessory to the right.
     *
     * RTL is only supported on Jelly Bean MR 1+.
     * http://android-developers.blogspot.com/2013/03/native-rtl-support-in-android-42.html
     */
    @Test
    @MediumTest
    @Feature({"keyboard-accessory"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.JELLY_BEAN_MR1)
    public void testSwitchFieldsRescrollsKeyboardAccessoryRtl()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage(true);
        DOMUtils.clickNode(mWebContentsRef.get(), "fn");
        CriteriaHelper.pollUiThread(new Criteria("Keyboard should be showing.") {
            @Override
            public boolean isSatisfied() {
                return UiUtils.isKeyboardShowing(
                        mActivityTestRule.getActivity(), mContainerRef.get());
            }
        });
        ThreadUtils.runOnUiThreadBlocking(() -> getSuggestionsComponent().scrollTo(-1000, 0));
        CriteriaHelper.pollUiThread(
                new Criteria("Last suggestion should be on the screen after manual scroll.") {
                    @Override
                    public boolean isSatisfied() {
                        View suggestion = getSuggestionAt(3);
                        if (suggestion != null) {
                            int[] location = new int[2];
                            suggestion.getLocationOnScreen(location);
                            return location[0] > 0;
                        } else {
                            return false;
                        }
                    }
                });
        // Simulates two clicks. Keyboard delay can often drops the first and doesn't set it again.
        // TODO(fhorschig): Remove safety net as soon as Accessory is decoupled from suggestions.
        DOMUtils.clickNode(mWebContentsRef.get(), "ln");
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        DOMUtils.clickNode(mWebContentsRef.get(), "ln");
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        CriteriaHelper.pollUiThread(
                new Criteria("Last suggestion should be off the screen after switching fields.") {
                    @Override
                    public boolean isSatisfied() {
                        View suggestion = getSuggestionAt(3);
                        if (suggestion != null) {
                            int[] location = new int[2];
                            suggestion.getLocationOnScreen(location);
                            return location[0] < 0;
                        } else {
                            return false;
                        }
                    }
                });
    }

    /**
     * Selecting a keyboard accessory suggestion should hide the keyboard and its keyboard
     * accessory.
     */
    @Test
    @MediumTest
    @Feature({"keyboard-accessory"})
    public void testSelectSuggestionHidesKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage(false);
        DOMUtils.clickNode(mWebContentsRef.get(), "fn");
        CriteriaHelper.pollUiThread(new Criteria("Keyboard should be showing.") {
            @Override
            public boolean isSatisfied() {
                return UiUtils.isKeyboardShowing(
                        mActivityTestRule.getActivity(), mContainerRef.get());
            }
        });
        ThreadUtils.runOnUiThreadBlocking(() -> {
            View suggestion = getSuggestionAt(0);
            if (suggestion != null) {
                suggestion.performClick();
            }
        });
        CriteriaHelper.pollUiThread(new Criteria("Keyboard should be hidden.") {
            @Override
            public boolean isSatisfied() {
                return !UiUtils.isKeyboardShowing(
                        mActivityTestRule.getActivity(), mContainerRef.get());
            }
        });
        Assert.assertTrue("Keyboard accessory should be hidden.",
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mKeyboardAccessoryRef.get().getVisibility() == View.GONE));
    }

    private AutofillKeyboardSuggestions getSuggestionsComponent() {
        // The view hierarchy:
        //   Keyboard accessory.
        //    \--> A list of accessory components.
        //        \--> A scroll view.
        //            \--> A list of suggestions.
        //                \--> A suggestion that can be clicked.
        ViewGroup keyboardAccessory = mKeyboardAccessoryRef.get();
        if (keyboardAccessory == null) return null; // It might still be loading, so don't assert!
        ViewGroup componentsList = (ViewGroup) mKeyboardAccessoryRef.get().getChildAt(0);
        if (componentsList == null) return null; // It might still be loading, so don't assert!
        ViewGroup scrollview = (ViewGroup) componentsList.getChildAt(0);
        if (scrollview == null) return null; // It might still be loading, so don't assert!
        return (AutofillKeyboardSuggestions) scrollview.getChildAt(0);
    }

    private View getSuggestionAt(int index) {
        ViewGroup suggestionsList = getSuggestionsComponent();
        if (suggestionsList == null) return null; // It might still be loading, so don't assert!
        return suggestionsList.getChildAt(index);
    }
}
