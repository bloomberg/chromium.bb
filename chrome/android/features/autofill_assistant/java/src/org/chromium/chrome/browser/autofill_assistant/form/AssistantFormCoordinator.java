// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import org.chromium.chrome.browser.autofill_assistant.AbstractListObserver;

/**
 * A coordinator responsible for showing a form to the user.
 *
 * See http://go/aa-form-action for more information.
 */
public class AssistantFormCoordinator {
    private final AssistantFormModel mModel;
    private final ScrollView mView;
    private final LinearLayout mContainerView;

    private boolean mForceInvisible;

    public AssistantFormCoordinator(
            Context context, AssistantFormModel model, int verticalSpacingPx) {
        mModel = model;
        mView = new ScrollView(context);
        mContainerView = new LinearLayout(context);
        mView.setLayoutParams(
                new LinearLayout.LayoutParams(/* width= */ ViewGroup.LayoutParams.MATCH_PARENT,
                        /* height= */ 0, /* weight= */ 1));
        mContainerView.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mContainerView.setOrientation(LinearLayout.VERTICAL);
        mView.addView(mContainerView);

        updateVisibility();
        mModel.getInputsModel().addObserver(new AbstractListObserver<Void>() {
            @Override
            public void onDataSetChanged() {
                mContainerView.removeAllViews();
                for (AssistantFormInput input : mModel.getInputsModel()) {
                    // Add the views to the linear layout (not the scroll view).
                    View view = input.createView(context, mContainerView);
                    mContainerView.addView(view);
                    ((LinearLayout.LayoutParams) view.getLayoutParams()).topMargin =
                            verticalSpacingPx;
                }
                updateVisibility();
            }
        });
    }

    /** Return the view associated to this coordinator. */
    public ScrollView getView() {
        return mView;
    }

    /** Force the view of this coordinator to be invisible. */
    public void setForceInvisible(boolean forceInvisible) {
        if (mForceInvisible == forceInvisible) return;

        mForceInvisible = forceInvisible;
        updateVisibility();
    }

    private void updateVisibility() {
        int visibility =
                !mForceInvisible && mModel.getInputsModel().size() > 0 ? View.VISIBLE : View.GONE;
        mView.setVisibility(visibility);
    }
}
