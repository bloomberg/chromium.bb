// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.test.suitebuilder.annotation.MediumTest;
import android.view.ViewGroup;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;
import org.chromium.ui.autofill.AutofillPopup;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests for interaction of the AutofillPopup and a keyboard.
 */
public class AutofillPopupWithKeyboardTest extends ChromeShellTestBase {
    /**
     * Test that showing autofill popup and keyboard will not hide the autofill popup.
     */
    @MediumTest
    @Feature({"autofill-keyboard"})
    public void testShowAutofillPopupAndKeyboardimultaneously()
            throws InterruptedException, ExecutionException, TimeoutException {
        launchChromeShellWithUrl(UrlUtils.encodeHtmlDataUri("<html><head>"
                + "<meta name=\"viewport\""
                + "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>"
                + "<body><form method=\"POST\">"
                + "<input type=\"text\" id=\"fn\" autocomplete=\"given-name\" /><br>"
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
        assertTrue(waitForActiveShellToBeDoneLoading());
        new AutofillTestHelper().setProfile(new AutofillProfile("", "https://www.example.com",
                "John Smith", "Acme Inc", "1 Main\nApt A", "CA", "San Francisco", "", "94102", "",
                "US", "(415) 888-9999", "john@acme.inc", "en"));
        final AtomicReference<ContentViewCore> viewCoreRef = new AtomicReference<ContentViewCore>();
        final AtomicReference<WebContents> webContentsRef = new AtomicReference<WebContents>();
        final AtomicReference<ViewGroup> viewRef = new AtomicReference<ViewGroup>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                viewCoreRef.set(getActivity().getActiveContentViewCore());
                webContentsRef.set(viewCoreRef.get().getWebContents());
                viewRef.set(viewCoreRef.get().getContainerView());
            }
        });
        assertTrue(DOMUtils.waitForNonZeroNodeBounds(webContentsRef.get(), "fn"));

        // Click on the unfocused input element for the first time to focus on it. This brings up
        // the keyboard, but does not show the autofill popup.
        DOMUtils.clickNode(this, viewCoreRef.get(), "fn");
        waitForKeyboardShown(true);

        // Hide the keyboard while still keeping the focus on the input field.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                UiUtils.hideKeyboard(viewRef.get());
            }
        });
        waitForKeyboardShown(false);

        // Click on the focused input element for the second time. This brings up the autofill popup
        // and shows the keyboard at the same time. Showing the keyboard should not hide the
        // autofill popup.
        DOMUtils.clickNode(this, viewCoreRef.get(), "fn");
        waitForKeyboardShown(true);

        // Verify that the autofill popup is showing.
        assertTrue("Autofill Popup anchor view was never added.",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return viewRef.get().findViewById(R.id.dropdown_popup_window) != null;
                    }
                }));
        Object popupObject = ThreadUtils.runOnUiThreadBlocking(new Callable<Object>() {
            @Override
            public Object call() {
                return viewRef.get().findViewById(R.id.dropdown_popup_window).getTag();
            }
        });
        assertTrue(popupObject instanceof AutofillPopup);
        final AutofillPopup popup = (AutofillPopup) popupObject;
        assertTrue("Autofill Popup was never shown.",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return popup.isShowing();
                    }
                }));
    }

    /**
     * Wait until the keyboard is showing and notify the ContentViewCore that its height was changed
     * on the UI thread.
     */
    private void waitForKeyboardShown(final boolean shown) throws InterruptedException {
        assertTrue((shown ? "Keyboard was never shown" : "Keyboard was never hidden"),
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return shown == UiUtils.isKeyboardShowing(
                                getActivity(),
                                getActivity().getActiveContentViewCore().getContainerView());
                    }
                }));
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ContentViewCore viewCore = getActivity().getActiveContentViewCore();
                viewCore.onSizeChanged(viewCore.getViewportWidthPix(),
                        viewCore.getViewportHeightPix() + (shown ? -100 : 100),
                        viewCore.getViewportWidthPix(), viewCore.getViewportHeightPix());
            }
        });
    }
}
