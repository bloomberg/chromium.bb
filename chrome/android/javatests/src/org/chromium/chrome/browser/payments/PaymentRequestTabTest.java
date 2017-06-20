// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for dismissing the dialog when the user switches tabs or navigates
 * elsewhere.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class PaymentRequestTabTest implements MainActivityStartCallback {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_dynamic_shipping_test.html", this);

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "jon.doe@google.com", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                billingAddressId, "" /* serverId */));
    }

    /** If the user switches tabs somehow, the dialog is dismissed. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testDismissOnTabSwitch()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(0, mPaymentRequestTestRule.getDismissed().getCallCount());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mPaymentRequestTestRule.getActivity().getTabCreator(false).createNewTab(
                        new LoadUrlParams("about:blank"), TabLaunchType.FROM_CHROME_UI, null);
            }
        });
        mPaymentRequestTestRule.getDismissed().waitForCallback(0);
    }

    /** If the user closes the tab, the dialog is dismissed. */
    //@MediumTest
    //@Feature({"Payments"})
    // Disabled due to recent flakiness: crbug.com/661450.
    @Test
    @DisabledTest
    public void testDismissOnTabClose()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(0, mPaymentRequestTestRule.getDismissed().getCallCount());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabModel currentModel = mPaymentRequestTestRule.getActivity().getCurrentTabModel();
                TabModelUtils.closeCurrentTab(currentModel);
            }
        });
        mPaymentRequestTestRule.getDismissed().waitForCallback(0);
    }

    /** If the user navigates anywhere, the dialog is dismissed. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testDismissOnTabNavigate()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(0, mPaymentRequestTestRule.getDismissed().getCallCount());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TabModel currentModel = mPaymentRequestTestRule.getActivity().getCurrentTabModel();
                TabModelUtils.getCurrentTab(currentModel).loadUrl(new LoadUrlParams("about:blank"));
            }
        });
        mPaymentRequestTestRule.getDismissed().waitForCallback(0);
    }
}
