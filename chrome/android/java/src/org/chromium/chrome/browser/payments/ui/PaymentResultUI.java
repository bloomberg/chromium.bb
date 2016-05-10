// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;

/**
 * Displays the status of a payment request to the user.
 */
public class PaymentResultUI {
    private final ViewGroup mResultLayout;

    /**
     * Constructs a UI that indicates the status of the payment request.
     *
     * Starts off with an indeterminate progress spinner.
     *
     * @param context Context to pull resources from.
     * @param title   Title of the webpage.
     * @param origin  Origin of the webpage.
     */
    public PaymentResultUI(Context context, String title, String origin) {
        mResultLayout =
                (ViewGroup) LayoutInflater.from(context).inflate(R.layout.payment_result, null);
        ((TextView) mResultLayout.findViewById(R.id.page_title)).setText(title);
        ((TextView) mResultLayout.findViewById(R.id.hostname)).setText(origin);
    }

    /**
     * Updates the UI to display whether or not the payment request was successful.
     *
     * @param paymentSuccess Whether or not the payment request was successful.
     * @param callback       Callback to run upon dismissal.
     */
    public void update(boolean paymentSuccess, final Runnable callback) {
        if (mResultLayout.getParent() == null || paymentSuccess) {
            // Dismiss the dialog immediately.
            callback.run();
        } else {
            // Show the result of the payment.
            Context context = mResultLayout.getContext();
            TextView resultMessage = (TextView) mResultLayout.findViewById(R.id.waiting_message);

            // Hide the progress bar.
            View progressBar = mResultLayout.findViewById(R.id.waiting_progress);
            progressBar.setVisibility(View.GONE);

            // Describe the error.
            resultMessage.setText(context.getString(R.string.payments_error_message));
            resultMessage.setTextColor(ApiCompatibilityUtils.getColor(
                    context.getResources(), R.color.error_text_color));

            // Make the user explicitly click on the OK button to dismiss the dialog.
            View confirmButton = mResultLayout.findViewById(R.id.ok_button);
            confirmButton.setVisibility(View.VISIBLE);
            confirmButton.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    callback.run();
                }
            });
        }
    }

    /** @return The Layout that represents the result UI. */
    public ViewGroup getView() {
        return mResultLayout;
    }

    /**
     * Sets what icon is displayed in the header.
     *
     * @param bitmap Icon to display.
     */
    public void setBitmap(Bitmap bitmap) {
        ((ImageView) mResultLayout.findViewById(R.id.icon_view)).setImageBitmap(bitmap);
    }

    /**
     * Computes the maximum possible width for a dialog box.
     *
     * Follows https://www.google.com/design/spec/components/dialogs.html#dialogs-simple-dialogs
     *
     * @param context         Context to pull resources from.
     * @param availableWidth  Available width for the dialog.
     * @param availableHeight Available height for the dialog.
     * @return Maximum possible width for the dialog box.
     *
     * TODO(dfalcantara): Revisit this function when the new assets come in.
     * TODO(dfalcantara): The dialog should listen for configuration changes and resize accordingly.
     */
    public static int computeMaxWidth(Context context, int availableWidth, int availableHeight) {
        int baseUnit = context.getResources().getDimensionPixelSize(R.dimen.dialog_width_unit);
        int maxSize = Math.min(availableWidth, availableHeight);
        int multiplier = maxSize / baseUnit;
        int floatingDialogWidth = multiplier * baseUnit;
        return floatingDialogWidth;
    }
}
