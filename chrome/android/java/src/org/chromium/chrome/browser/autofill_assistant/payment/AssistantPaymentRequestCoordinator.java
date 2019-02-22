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
    @Nullable
    private Runnable mOnVisibilityChanged;
    private final ViewGroup mView;

    private AssistantPaymentRequestDelegate mDelegate;
    private AutofillAssistantPaymentRequest mPaymentRequest;

    public AssistantPaymentRequestCoordinator(Context context, AssistantPaymentRequestModel model) {
        // TODO(crbug.com/806868): Remove this.
        mView = new LinearLayout(context);
        mView.addView(new View(context));

        // Payment request is initially hidden.
        setVisible(false);

        // Listen for model changes.
        model.addObserver((source, propertyKey) -> {
            if (AssistantPaymentRequestModel.DELEGATE == propertyKey) {
                mDelegate = model.get(AssistantPaymentRequestModel.DELEGATE);
            } else if (AssistantPaymentRequestModel.WEB_CONTENTS == propertyKey
                    || AssistantPaymentRequestModel.OPTIONS == propertyKey) {
                resetView(model.get(AssistantPaymentRequestModel.WEB_CONTENTS),
                        model.get(AssistantPaymentRequestModel.OPTIONS));
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

    private void resetView(
            @Nullable WebContents webContents, @Nullable AssistantPaymentRequestOptions options) {
        if (mPaymentRequest != null) {
            mPaymentRequest.close();
            mPaymentRequest = null;
        }
        if (options == null || webContents == null) {
            setVisible(false);
            return;
        }
        mPaymentRequest = new AutofillAssistantPaymentRequest(webContents, options);
        mPaymentRequest.show(mView.getChildAt(0), selectedPaymentInformation -> {
            if (mDelegate != null) {
                mDelegate.onPaymentInformationSelected(selectedPaymentInformation);
            }
        });
        setVisible(true);
    }
}
