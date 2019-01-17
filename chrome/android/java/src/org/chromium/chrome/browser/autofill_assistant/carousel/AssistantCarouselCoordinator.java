// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.carousel;

import android.content.Context;
import android.graphics.Rect;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.TypedValue;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Coordinator responsible for suggesting chips to the user.
 */
public class AssistantCarouselCoordinator {
    // TODO(crbug.com/806868): Get those from XML.
    private static final int CHIPS_INNER_SPACING_DP = 16;
    private static final int CHIPS_OUTER_SPACING_DP = 24;

    private final Runnable mOnVisibilityChanged;

    private final LinearLayoutManager mLayoutManager;
    private final RecyclerView mView;
    private final ListModel<AssistantChip> mModel = new ListModel<>();

    public AssistantCarouselCoordinator(Context context, Runnable onVisibilityChanged) {
        mOnVisibilityChanged = onVisibilityChanged;

        mLayoutManager = new LinearLayoutManager(
                context, LinearLayoutManager.HORIZONTAL, /* reverseLayout= */ false);
        mView = new RecyclerView(context);
        mView.setLayoutManager(mLayoutManager);
        mView.addItemDecoration(new SpaceItemDecoration(context));
        mView.getItemAnimator().setAddDuration(0);
        mView.getItemAnimator().setChangeDuration(0);
        mView.getItemAnimator().setMoveDuration(0);
        mView.getItemAnimator().setRemoveDuration(0);
        mView.setAdapter(new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(
                        mModel, AssistantChip::getType, AssistantChipViewHolder::bind),
                AssistantChipViewHolder::create));
    }

    /**
     * Return the view associated to this carousel.
     */
    public RecyclerView getView() {
        return mView;
    }

    /**
     * Show or hide this carousel within its parent and call the {@code mOnVisibilityChanged}
     * listener.
     */
    public void setVisible(boolean visible) {
        int visibility = visible ? View.VISIBLE : View.GONE;
        boolean changed = mView.getVisibility() != visibility;
        if (changed) {
            mView.setVisibility(visibility);
            mOnVisibilityChanged.run();
        }
    }

    /**
     * Set the chips to show.
     */
    public void setChips(List<AssistantChip> chips) {
        if (chips.isEmpty()) {
            setVisible(false);
        } else {
            setVisible(true);
        }
        mLayoutManager.setReverseLayout(shouldReverseLayout(chips));
        mModel.set(chips);
    }

    /**
     * Remove all chips shown to the user.
     */
    public void clearChips() {
        setChips(Collections.emptyList());
    }

    /**
     * Set the chips to show. The chip {@code i} should have text {@code texts[i]} and should be
     * represented as a highlighted button iff {@code highlights[i]} is true (hence {@code texts}
     * and {@code highlights} should have the same length.
     * When {@code chip[i]} is clicked, {@code callback} will be called with the value i.
     */
    public void setChips(String[] texts, boolean[] highlights, Callback<Integer> callback) {
        assert texts.length == highlights.length;
        if (texts.length == 0) {
            clearChips();
            return;
        }

        // TODO(crbug.com/806868): Move the logic somewhere else?
        List<AssistantChip> chips = new ArrayList<>();
        int nonHighlightType = anyIsTrue(highlights) ? AssistantChip.TYPE_BUTTON_TEXT
                                                     : AssistantChip.TYPE_CHIP_ASSISTIVE;
        for (int i = 0; i < texts.length; i++) {
            int type = highlights[i] ? AssistantChip.TYPE_BUTTON_FILLED_BLUE : nonHighlightType;
            int index = i;
            chips.add(new AssistantChip(type, texts[i], () -> {
                clearChips();
                callback.onResult(index);
            }));
        }
        setChips(chips);
    }

    private boolean anyIsTrue(boolean[] flags) {
        for (int i = 0; i < flags.length; i++) {
            if (flags[i]) {
                return true;
            }
        }
        return false;
    }

    private boolean shouldReverseLayout(List<AssistantChip> chips) {
        for (int i = 0; i < chips.size(); i++) {
            if (chips.get(i).getType() != AssistantChip.TYPE_CHIP_ASSISTIVE) {
                return true;
            }
        }
        return false;
    }

    private class SpaceItemDecoration extends RecyclerView.ItemDecoration {
        private final int mInnerSpacePx;
        private final int mOuterSpacePx;

        SpaceItemDecoration(Context context) {
            mInnerSpacePx = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                    CHIPS_INNER_SPACING_DP / 2, context.getResources().getDisplayMetrics());
            mOuterSpacePx = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                    CHIPS_OUTER_SPACING_DP, context.getResources().getDisplayMetrics());
        }

        @Override
        public void getItemOffsets(
                Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
            int position = parent.getChildAdapterPosition(view);
            int left;
            int right;
            if (position == 0) {
                left = mOuterSpacePx;
            } else {
                left = mInnerSpacePx;
            }

            if (position == parent.getAdapter().getItemCount() - 1) {
                right = mOuterSpacePx;
            } else {
                right = mInnerSpacePx;
            }

            if (!mLayoutManager.getReverseLayout()) {
                outRect.left = left;
                outRect.right = right;
            } else {
                outRect.left = right;
                outRect.right = left;
            }
        }
    }
}
