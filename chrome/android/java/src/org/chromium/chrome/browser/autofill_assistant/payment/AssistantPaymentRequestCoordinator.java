// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.content_public.browser.WebContents;

// TODO(crbug.com/806868): Refactor AutofillAssistantPaymentRequest and merge with this file.
// TODO(crbug.com/806868): Use mCarouselCoordinator to show chips.
/**
 * Coordinator for the Payment Request.
 */
public class AssistantPaymentRequestCoordinator {
    private final WebContents mWebContents;
    @Nullable
    private Runnable mOnVisibilityChanged;
    private final ViewGroup mView;

    private AssistantPaymentRequestDelegate mDelegate;

    public AssistantPaymentRequestCoordinator(
            Context context, WebContents webContents, AssistantPaymentRequestModel model) {
        mWebContents = webContents;
        assert webContents != null;

        // TODO(crbug.com/806868): Remove this.
        mView = new LinearLayout(context);
        mView.addView(new View(context));

        // Payment request is initially hidden.
        setVisible(false);

        // Listen for model changes.
        model.addObserver((source, propertyKey) -> {
            if (AssistantPaymentRequestModel.DELEGATE == propertyKey) {
                mDelegate = model.get(AssistantPaymentRequestModel.DELEGATE);
            } else if (AssistantPaymentRequestModel.OPTIONS == propertyKey) {
                AssistantPaymentRequestOptions options =
                        model.get(AssistantPaymentRequestModel.OPTIONS);
                if (options != null) {
                    resetView(options);
                    setVisible(true);
                } else {
                    setVisible(false);
                }
            }
        });
    }

    public View getView() {
        return mView;
    }

    private void setVisible(boolean visible) {
        int visibility = visible ? View.VISIBLE : View.GONE;
        boolean changed = mView.getVisibility() != visibility;
        if (changed) {
            mView.setVisibility(visibility);
            if (mOnVisibilityChanged != null) {
                mOnVisibilityChanged.run();
            }
        }
    }

    /**
     * Set the listener that should be triggered when changing the listener of this coordinator
     * view.
     */
    public void setVisibilityChangedListener(Runnable listener) {
        mOnVisibilityChanged = listener;
    }

    private void resetView(AssistantPaymentRequestOptions options) {
        AutofillAssistantPaymentRequest paymentRequest =
                new AutofillAssistantPaymentRequest(mWebContents, options);
        paymentRequest.show(mView.getChildAt(0), selectedPaymentInformation -> {
            if (mDelegate != null) {
                mDelegate.onPaymentInformationSelected(selectedPaymentInformation);
            }
        });
    }
}
