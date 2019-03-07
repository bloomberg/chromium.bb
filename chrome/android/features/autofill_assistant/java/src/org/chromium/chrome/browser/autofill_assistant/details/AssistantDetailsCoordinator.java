// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.details;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsViewBinder.ViewHolder;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator responsible for showing details.
 */
public class AssistantDetailsCoordinator {
    private final View mView;

    public AssistantDetailsCoordinator(Context context, AssistantDetailsModel model) {
        mView = LayoutInflater.from(context).inflate(
                R.layout.autofill_assistant_details, /* root= */ null);
        ViewHolder viewHolder = new ViewHolder(context, mView);
        AssistantDetailsViewBinder viewBinder = new AssistantDetailsViewBinder(context);
        PropertyModelChangeProcessor.create(model, viewHolder, viewBinder);

        // Details view is initially hidden.
        setVisible(false);

        // Observe details in model to hide or show this coordinator view.
        model.addObserver((source, propertyKey) -> {
            if (AssistantDetailsModel.DETAILS == propertyKey) {
                AssistantDetails details = model.get(AssistantDetailsModel.DETAILS);
                if (details != null) {
                    setVisible(true);
                } else {
                    setVisible(false);
                }
            }
        });
    }

    /**
     * Return the view associated to the details.
     */
    public View getView() {
        return mView;
    }

    /**
     * Show or hide the details within its parent and call the {@code mOnVisibilityChanged}
     * listener.
     */
    private void setVisible(boolean visible) {
        int visibility = visible ? View.VISIBLE : View.GONE;
        if (mView.getVisibility() != visibility) {
            mView.setVisibility(visibility);
        }
    }
}
