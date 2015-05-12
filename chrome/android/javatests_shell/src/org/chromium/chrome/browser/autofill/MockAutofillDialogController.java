// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import junit.framework.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.autofill.AutofillDialogControllerAndroid.AutofillDialog;
import org.chromium.chrome.browser.autofill.AutofillDialogControllerAndroid.AutofillDialogDelegate;
import org.chromium.chrome.browser.autofill.AutofillDialogControllerAndroid.AutofillDialogFactory;
import org.chromium.ui.base.WindowAndroid;

/**
 * Mock of the AutofillDialog that returns a predefined set of values.
*/
public class MockAutofillDialogController implements AutofillDialog {
    private final AutofillDialogResult.ResultWallet mResult;
    private final boolean mLastUsedChoiceIsAutofill;
    private final String mLastUsedAccountName;
    private final String mGuidDefaultBilling;
    private final String mGuidDefaultShipping;
    private final String mGuidDefaultCard;

    private AutofillDialogDelegate mDelegate;

    /**
     * Installs the mock AutofillDialog implementation.
     */
    public static void installMockFactory(
            final long delayMillis,
            final AutofillDialogResult.ResultWallet result,
            final boolean lastUsedChoiceIsAutofill,
            final String lastUsedAccountName,
            final String guidDefaultBilling,
            final String guidDefaultShipping,
            final String guidDefaultCard,
            final boolean shouldRequestFullBilling,
            final boolean shouldRequestShipping,
            final boolean shouldRequestPhoneNumbers) {
        AutofillDialogControllerAndroid.setDialogFactory(new AutofillDialogFactory() {
            @Override
            public AutofillDialog createDialog(
                    final AutofillDialogDelegate delegate,
                    final WindowAndroid windowAndroid,
                    final boolean requestFullBillingAddress,
                    final boolean requestShippingAddress,
                    final boolean requestPhoneNumbers,
                    final boolean incognitoMode,
                    final boolean initialChoiceIsAutofill,
                    final String initialWalletAccountName,
                    final String initialBillingGuid,
                    final String initialShippingGuid,
                    final String initialCardGuid,
                    final String merchantDomain,
                    final String[] shippingCountries,
                    final String[] creditCardTypes) {
                Assert.assertEquals("Full billing details flag doesn't match",
                        shouldRequestFullBilling, requestFullBillingAddress);
                Assert.assertEquals("Shipping details flag doesn't match",
                        shouldRequestShipping, requestShippingAddress);
                Assert.assertEquals("Phone numbers flag doesn't match",
                        shouldRequestPhoneNumbers, requestPhoneNumbers);
                return new MockAutofillDialogController(
                        delegate,
                        delayMillis,
                        result,
                        lastUsedChoiceIsAutofill,
                        lastUsedAccountName,
                        guidDefaultBilling,
                        guidDefaultShipping,
                        guidDefaultCard,
                        windowAndroid,
                        requestFullBillingAddress, requestShippingAddress,
                        requestPhoneNumbers,
                        incognitoMode,
                        initialChoiceIsAutofill, initialWalletAccountName,
                        initialBillingGuid, initialShippingGuid, initialCardGuid,
                        merchantDomain,
                        shippingCountries,
                        creditCardTypes);
            }
        });
    }

    private MockAutofillDialogController(
            final AutofillDialogControllerAndroid.AutofillDialogDelegate delegate,
            final long delayMillis,
            final AutofillDialogResult.ResultWallet result,
            final boolean lastUsedChoiceIsAutofill,
            final String lastUsedAccountName,
            final String guidDefaultBilling,
            final String guidDefaultShipping,
            final String guidDefaultCard,
            final WindowAndroid windowAndroid,
            final boolean requestFullBillingAddress, final boolean requestShippingAddress,
            final boolean requestPhoneNumbers,
            final boolean incognitoMode,
            final boolean initialChoiceIsAutofill, final String initialWalletAccountName,
            final String initialBillingGuid, final String initialShippingGuid,
            final String initialCardGuid,
            final String merchantDomain,
            final String[] shippingCountries,
            final String[] creditCardTypes) {
        mDelegate = delegate;
        mResult = result;
        mLastUsedChoiceIsAutofill = lastUsedChoiceIsAutofill;
        mLastUsedAccountName = lastUsedAccountName;
        mGuidDefaultBilling = guidDefaultBilling;
        mGuidDefaultShipping = guidDefaultShipping;
        mGuidDefaultCard = guidDefaultCard;
        ThreadUtils.postOnUiThreadDelayed(new Runnable() {
            @Override
            public void run() {
                triggerResultCallback();
            }
        }, delayMillis);
    }

    // AutofillDialog implementation:

    @Override
    public void onDestroy() {
        mDelegate = null;
    }

    public void triggerResultCallback() {
        if (mDelegate == null) return;

        if (mResult == null) {
            mDelegate.dialogCancel();
            return;
        }

        mDelegate.dialogContinue(
                mResult,
                mLastUsedChoiceIsAutofill, mLastUsedAccountName,
                mGuidDefaultBilling, mGuidDefaultShipping, mGuidDefaultCard);
    }
}
