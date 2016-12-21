// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for dismissing the dialog when the user switches tabs or navigates
 * elsewhere.
 */
public class PaymentRequestTabTest extends PaymentRequestTestBase {
    public PaymentRequestTabTest() {
        super("payment_request_dynamic_shipping_test.html");
    }

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "jon.doe@google.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.pr_visa,
                billingAddressId, "" /* serverId */));
    }

    /** If the user switches tabs somehow, the dialog is dismissed. */
    @MediumTest
    @Feature({"Payments"})
    public void testDismissOnTabSwitch() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mReadyForInput);
        assertEquals(0, mDismissed.getCallCount());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().getTabCreator(false).createNewTab(
                        new LoadUrlParams("about:blank"), TabLaunchType.FROM_CHROME_UI, null);
            }
        });
        mDismissed.waitForCallback(0);
    }

    /** If the user closes the tab, the dialog is dismissed. */
    //@MediumTest
    //@Feature({"Payments"})
    // Disabled due to recent flakiness: crbug.com/661450.
    @DisabledTest
    public void testDismissOnTabClose() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mReadyForInput);
        assertEquals(0, mDismissed.getCallCount());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabModel currentModel = getActivity().getCurrentTabModel();
                TabModelUtils.closeCurrentTab(currentModel);
            }
        });
        mDismissed.waitForCallback(0);
    }

    /** If the user navigates anywhere, the dialog is dismissed. */
    @MediumTest
    @Feature({"Payments"})
    public void testDismissOnTabNavigate() throws InterruptedException, ExecutionException,
            TimeoutException {
        triggerUIAndWait(mReadyForInput);
        assertEquals(0, mDismissed.getCallCount());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabModel currentModel = getActivity().getCurrentTabModel();
                TabModelUtils.getCurrentTab(currentModel).loadUrl(new LoadUrlParams("about:blank"));
            }
        });
        mDismissed.waitForCallback(0);
    }
}
