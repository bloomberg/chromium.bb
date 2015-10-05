// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests for autofill keyboard accessory.
 */
@CommandLineFlags.Add({ChromeSwitches.ENABLE_AUTOFILL_KEYBOARD_ACCESSORY})
public class AutofillKeyboardAccessoryTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private final AtomicReference<ContentViewCore> mViewCoreRef =
            new AtomicReference<ContentViewCore>();
    private final AtomicReference<ViewGroup> mContainerRef = new AtomicReference<ViewGroup>();
    private final AtomicReference<ViewGroup> mKeyboardAccessoryRef =
            new AtomicReference<ViewGroup>();

    public AutofillKeyboardAccessoryTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL(UrlUtils.encodeHtmlDataUri("<html><head>"
                + "<meta name=\"viewport\""
                + "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>"
                + "<body><form method=\"POST\">"
                + "<input type=\"text\" id=\"fn\" autocomplete=\"given-name\" autofocus/><br>"
                + "<input type=\"text\" id=\"ln\" autocomplete=\"family-name\" /><br>"
                + "<textarea id=\"sa\" autocomplete=\"street-address\"></textarea><br>"
                + "<input type=\"text\" id=\"a1\" autocomplete=\"address-line1\" /><br>"
                + "<input type=\"text\" id=\"a2\" autocomplete=\"address-line2\" /><br>"
                + "<input type=\"text\" id=\"ct\" autocomplete=\"locality\" /><br>"
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
        try {
            new AutofillTestHelper().setProfile(new AutofillProfile("", "https://www.example.com",
                    "John Smith", "Acme Inc", "1 Main\nApt A", "CA", "San Francisco", "", "94102",
                    "", "US", "(415) 888-9999", "john@acme.inc", "en"));
        } catch (ExecutionException e) {
            assertTrue("Could not set an autofill profile.", false);
        }

        final AtomicReference<WebContents> webContentsRef = new AtomicReference<WebContents>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mViewCoreRef.set(getActivity().getCurrentContentViewCore());
                webContentsRef.set(mViewCoreRef.get().getWebContents());
                mContainerRef.set(mViewCoreRef.get().getContainerView());
                mKeyboardAccessoryRef.set(
                        getActivity().getWindowAndroid().getKeyboardAccessoryView());
            }
        });
        assertTrue(DOMUtils.waitForNonZeroNodeBounds(webContentsRef.get(), "fn"));
    }

    /**
     * Autofocused fields should not show a keyboard accessory.
     */
    @MediumTest
    @Feature({"keyboard-accessory"})
    public void testAutofocusedFieldDoesNotShowKeyboardAccessory() throws ExecutionException {
        assertTrue("Keyboard accessory should be hidden.",
                ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return mKeyboardAccessoryRef.get().getVisibility() == View.GONE;
                    }
                }).booleanValue());
    }

    /**
     * Tapping on an input field should show a keyboard and its keyboard accessory.
     */
    @MediumTest
    @Feature({"keyboard-accessory"})
    public void testTapInputFieldShowsKeyboardAccessory() throws ExecutionException,
             InterruptedException, TimeoutException {
        DOMUtils.clickNode(this, mViewCoreRef.get(), "fn");
        assertTrue("Keyboard should be showing.",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return UiUtils.isKeyboardShowing(getActivity(), mContainerRef.get());
                    }
                }));
        assertTrue("Keyboard accessory should be showing.",
                ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return mKeyboardAccessoryRef.get().getVisibility() == View.VISIBLE;
                    }
                }).booleanValue());
    }

    /**
     * Selecting a keyboard accessory suggestion should hide the keyboard and its keyboard
     * accessory.
     */
    @MediumTest
    @Feature({"keyboard-accessory"})
    public void testSelectSuggestionHidesKeyboardAccessory() throws ExecutionException,
             InterruptedException, TimeoutException {
        DOMUtils.clickNode(this, mViewCoreRef.get(), "fn");
        assertTrue("Keyboard should be showing.",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return UiUtils.isKeyboardShowing(getActivity(), mContainerRef.get());
                    }
                }));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // The view hierarchy:
                //   Keyboard accessory.
                //    \--> A list of suggestions.
                //          \--> A suggestion that can be clicked.
                ((ViewGroup) mKeyboardAccessoryRef.get().getChildAt(0))
                        .getChildAt(0)
                        .performClick();
            }
        });
        assertTrue("Keyboard should be hidden.",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return !UiUtils.isKeyboardShowing(getActivity(), mContainerRef.get());
                    }
                }));
        assertTrue("Keyboard accessory should be hidden.",
                ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return mKeyboardAccessoryRef.get().getVisibility() == View.GONE;
                    }
                }).booleanValue());
    }
}
