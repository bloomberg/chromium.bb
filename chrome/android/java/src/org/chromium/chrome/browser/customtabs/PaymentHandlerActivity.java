// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.view.Gravity;
import android.view.WindowManager;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.ServiceWorkerPaymentAppBridge;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Simple wrapper around CustomTabActivity to be used when launching a payment handler tab, which
 * uses a different theme.
 */
public class PaymentHandlerActivity extends CustomTabActivity {
    private static final double BOTTOM_SHEET_HEIGHT_RATIO = 0.7;
    private boolean mHaveNotifiedServiceWorker;

    @Override
    protected void initializeMainTab(Tab tab) {
        super.initializeMainTab(tab);
        ServiceWorkerPaymentAppBridge.addTabObserverForPaymentRequestTab(tab);
    }

    @Override
    public void preInflationStartup() {
        super.preInflationStartup();

        int heightInPhysicalPixels = (int) (getWindowAndroid().getDisplay().getDisplayHeight()
                * BOTTOM_SHEET_HEIGHT_RATIO);
        int minimumHeightInPhysicalPixels = getResources().getDimensionPixelSize(
                R.dimen.payments_handler_window_minimum_height);
        if (heightInPhysicalPixels < minimumHeightInPhysicalPixels)
            heightInPhysicalPixels = minimumHeightInPhysicalPixels;

        WindowManager.LayoutParams attributes = getWindow().getAttributes();
        attributes.height = heightInPhysicalPixels;
        attributes.gravity = Gravity.BOTTOM;
        getWindow().setAttributes(attributes);
    }

    @Override
    protected void handleFinishAndClose() {
        // Notify the window is closing so as to abort invoking payment app early.
        if (!mHaveNotifiedServiceWorker && getActivityTab().getWebContents() != null) {
            mHaveNotifiedServiceWorker = true;
            ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(
                    getActivityTab().getWebContents());
        }

        super.handleFinishAndClose();
    }
}