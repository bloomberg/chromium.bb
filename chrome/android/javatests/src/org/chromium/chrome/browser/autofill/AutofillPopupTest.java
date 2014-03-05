// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.test.suitebuilder.annotation.MediumTest;
import android.text.TextUtils;
import android.view.View;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.ui.autofill.AutofillPopup;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the AutofillPopup.
 */
public class AutofillPopupTest extends ChromeShellTestBase {

    private static final String FIRST_NAME = "John";
    private static final String LAST_NAME = "Smith";
    private static final String COMPANY_NAME = "Acme Inc.";
    private static final String ADDRESS_LINE1 = "1 Main";
    private static final String ADDRESS_LINE2 = "Apt A";
    private static final String CITY = "San Francisco";
    private static final String STATE = "CA";
    private static final String ZIP_CODE = "94102";
    private static final String COUNTRY = "US";
    private static final String PHONE_NUMBER = "4158889999";
    private static final String EMAIL = "john@acme.inc";
    private static final String ORIGIN = "https://www.example.com";

    private static final String PAGE_DATA = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\"" +
            "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>" +
            "<body><form method=\"POST\">" +
            "<input type=\"text\" id=\"fn\" autocomplete=\"given-name\"><br>" +
            "<input type=\"text\" id=\"ln\" autocomplete=\"family-name\"><br>" +
            "<input type=\"text\" id=\"a1\" autocomplete=\"address-line1\"><br>" +
            "<input type=\"text\" id=\"a2\" autocomplete=\"address-line2\"><br>" +
            "<input type=\"text\" id=\"ct\" autocomplete=\"locality\"><br>" +
            "<input type=\"text\" id=\"zc\" autocomplete=\"postal-code\"><br>" +
            "<input type=\"text\" id=\"em\" autocomplete=\"email\"><br>" +
            "<input type=\"text\" id=\"ph\" autocomplete=\"tel\"><br>" +
            "<input type=\"submit\" />" +
            "</form></body></html>");

    private AutofillTestHelper mHelper;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        clearAppData();
        launchChromeShellWithUrl(PAGE_DATA);
        assertTrue(waitForActiveShellToBeDoneLoading());
        mHelper = new AutofillTestHelper();
    }

    /**
     * Tests that bringing up an Autofill and clicking on the first entry fills out the expected
     * Autofill information.
     */
    @MediumTest
    @Feature({"autofill"})
    public void testClickAutofillPopupSuggestion()
            throws InterruptedException, ExecutionException, TimeoutException {
        // The TestInputMethodManagerWrapper intercepts showSoftInput so that a keyboard is never
        // brought up.
        final ContentView view = getActivity().getActiveContentView();
        final TestInputMethodManagerWrapper immw =
                new TestInputMethodManagerWrapper(view.getContentViewCore());
        view.getContentViewCore().getImeAdapterForTest().setInputMethodManagerWrapper(immw);

        // Add an Autofill profile.
        AutofillProfile profile = new AutofillProfile(
                "" /* guid */, ORIGIN, FIRST_NAME + " " + LAST_NAME, COMPANY_NAME, ADDRESS_LINE1,
                ADDRESS_LINE2, CITY, STATE, ZIP_CODE, COUNTRY, PHONE_NUMBER, EMAIL);
        mHelper.setProfile(profile);
        assertEquals(1, mHelper.getNumberOfProfiles());

        // Click the input field for the first name.
        final TestCallbackHelperContainer viewClient = new TestCallbackHelperContainer(view);
        assertTrue(DOMUtils.waitForNonZeroNodeBounds(view, viewClient, "fn"));
        DOMUtils.clickNode(this, view, viewClient, "fn");

        waitForKeyboardShowRequest(immw, 1);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                view.getContentViewCore().getInputConnectionForTest().setComposingText("J", 1);
            }
        });

        waitForAnchorViewAdd(view);
        View anchorView = view.findViewById(R.id.autofill_popup_window);

        assertTrue(anchorView.getTag() instanceof AutofillPopup);
        final AutofillPopup popup = (AutofillPopup) anchorView.getTag();

        waitForAutofillPopopShow(popup);

        TouchCommon touchCommon = new TouchCommon(this);
        touchCommon.singleClickViewRelative(popup.getListView(), 10, 10);

        waitForInputFieldFill(view, viewClient);

        assertEquals("First name did not match",
                FIRST_NAME, DOMUtils.getNodeValue(view, viewClient, "fn"));
        assertEquals("Last name did not match",
                LAST_NAME, DOMUtils.getNodeValue(view, viewClient, "ln"));
        assertEquals("Address line 1 did not match",
                ADDRESS_LINE1, DOMUtils.getNodeValue(view, viewClient, "a1"));
        assertEquals("Address line 2 did not match",
                ADDRESS_LINE2, DOMUtils.getNodeValue(view, viewClient, "a2"));
        assertEquals("City did not match",
                CITY, DOMUtils.getNodeValue(view, viewClient, "ct"));
        assertEquals("Zip code des not match",
                ZIP_CODE, DOMUtils.getNodeValue(view, viewClient, "zc"));
        assertEquals("Email does not match",
                EMAIL, DOMUtils.getNodeValue(view, viewClient, "em"));
        assertEquals("Phone number does not match",
                PHONE_NUMBER, DOMUtils.getNodeValue(view, viewClient, "ph"));
    }

    // Wait and assert helper methods -------------------------------------------------------------

    private void waitForKeyboardShowRequest(final TestInputMethodManagerWrapper immw,
            final int count) throws InterruptedException {
        assertTrue("Keyboard was never requested to be shown.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return immw.getShowSoftInputCounter() == count;
                    }
                }));
    }

    private void waitForAnchorViewAdd(final ContentView view) throws InterruptedException {
        assertTrue("Autofill Popup anchor view was never added.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return view.findViewById(R.id.autofill_popup_window) != null;
                    }
                }));
    }

    private void waitForAutofillPopopShow(final AutofillPopup popup) throws InterruptedException {
        assertTrue("Autofill Popup anchor view was never added.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return popup.isShowing();
                    }
                }));
    }

    private void waitForInputFieldFill(final ContentView view,
            final TestCallbackHelperContainer viewClient) throws InterruptedException {
        assertTrue("First name field was never filled.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        try {
                            return TextUtils.equals(FIRST_NAME,
                                    DOMUtils.getNodeValue(view, viewClient, "fn"));
                        } catch (InterruptedException e) {
                            return false;
                        } catch (TimeoutException e) {
                            return false;
                        }

                    }
                }));
    }
}
