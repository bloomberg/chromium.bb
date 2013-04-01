// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import java.util.ArrayList;
import java.util.List;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.view.animation.AnimationSet;
import android.view.animation.ScaleAnimation;
import android.view.animation.TranslateAnimation;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.AdapterView.OnItemSelectedListener;

import static org.chromium.chrome.browser.autofill.AutofillDialogConstants.NUM_SECTIONS;
import static org.chromium.chrome.browser.autofill.AutofillDialogConstants.SECTION_CC;
import static org.chromium.chrome.browser.autofill.AutofillDialogConstants.SECTION_CC_BILLING;
import static org.chromium.chrome.browser.autofill.AutofillDialogConstants.SECTION_BILLING;
import static org.chromium.chrome.browser.autofill.AutofillDialogConstants.SECTION_SHIPPING;

import org.chromium.chrome.R;

/**
 * This is the parent layout that contains the layouts for different states of
 * autofill dialog. In principle it shouldn't contain any logic related with the
 * actual workflow, but rather respond to any UI update messages coming to it
 * from the AutofillDialog.
 */
public class AutofillDialogContentView extends LinearLayout {
    private static final int ANIMATION_DURATION_MS = 1000;
    // TODO(yusufo): Remove all placeholders here and also in related layout xml files.
    static final int INVALID_LAYOUT = -1;
    static final int LAYOUT_EDITING_SHIPPING = 0;
    static final int LAYOUT_EDITING_CC = 1;
    static final int LAYOUT_EDITING_BILLING = 2;
    static final int LAYOUT_EDITING_CC_BILLING = 3;
    static final int LAYOUT_FETCHING = 4;
    static final int LAYOUT_STEADY = 5;

    private final Runnable mDismissSteadyLayoutRunnable = new Runnable() {
        @Override
        public void run() {
            mSteadyLayout.setVisibility(GONE);
        }
    };
    private Spinner[] mSpinners = new Spinner[NUM_SECTIONS];
    private AutofillDialogMenuAdapter[] mAdapters = new AutofillDialogMenuAdapter[NUM_SECTIONS];
    private ViewGroup mSteadyLayout;
    private ViewGroup[] mEditLayouts = new ViewGroup[NUM_SECTIONS];
    private int mCurrentLayout = -1;

    public AutofillDialogContentView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate () {
        mSteadyLayout = (ViewGroup) findViewById(R.id.general_layout);

        for (int i = 0; i < AutofillDialogConstants.NUM_SECTIONS; i++) {
            mEditLayouts[i] = (ViewGroup) findViewById(
                    AutofillDialogUtils.getLayoutIDForSection(i));
            int id = AutofillDialogUtils.getSpinnerIDForSection(i);
            mSpinners[i] = (Spinner) findViewById(id);
            AutofillDialogMenuAdapter adapter = new AutofillDialogMenuAdapter(getContext(),
                    new ArrayList<AutofillDialogMenuItem>());
            mAdapters[i] = adapter;
            if (id == AutofillDialogUtils.INVALID_ID) continue;
            mSpinners[i].setAdapter(adapter);
        }

        changeLayoutTo(LAYOUT_FETCHING);
    }

    /**
     * Initializes the labels for each section.
     * @param labels The label strings to be used for each section.
     */
    public void initializeLabelsForEachSection(String[] labels) {
        TextView labelView;
        for (int i = 0; i < labels.length && i < NUM_SECTIONS; i++) {
            labelView = (TextView) findViewById(AutofillDialogUtils.getLabelIDForSection(i));
            labelView.setText(labels[i]);
        }
    }

    @Override
    protected void onMeasure (int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        Spinner ccSpinner = mSpinners[SECTION_CC_BILLING];
        Spinner addressSpinner = mSpinners[SECTION_SHIPPING];
        ccSpinner.setDropDownWidth(ccSpinner.getMeasuredWidth());
        ccSpinner.setDropDownVerticalOffset(-ccSpinner.getMeasuredHeight());
        addressSpinner.setDropDownWidth(addressSpinner.getMeasuredWidth());
        addressSpinner.setDropDownVerticalOffset(-addressSpinner.getMeasuredHeight());
    }

    /**
     * Set the listener for all the dropdown members in the layout.
     * @param listener The listener object to attach to the dropdowns.
     */
    public void setOnItemSelectedListener(OnItemSelectedListener listener) {
        for (int i = 0; i < NUM_SECTIONS; i++) {
            if (mSpinners[i] != null) mSpinners[i].setOnItemSelectedListener(listener);
        }
    }

    /**
     * @return Whether the current layout is one of the editing layouts.
     */
    public boolean isInEditingMode() {
        return mCurrentLayout != INVALID_LAYOUT &&
                mCurrentLayout != LAYOUT_STEADY &&
                        mCurrentLayout != LAYOUT_FETCHING;
    }

    /**
     * @return The current section if we are in editing mode, INVALID_SECTION otherwise.
     */
    public int getCurrentSection() {
        return getSectionForLayoutMode(mCurrentLayout);
    }

    /**
     * Updates a dropdown with the given items and adds default items to the end.
     * @param items The {@link AutofillDialogMenuItem} array to update the dropdown with.
     */
    public void updateMenuItemsForSection(int section, List<AutofillDialogMenuItem> items) {
        AutofillDialogMenuAdapter adapter = mAdapters[section];
        adapter.clear();
        adapter.addAll(items);
    }

    /**
     * Transitions the layout shown to a given layout.
     * @param mode The layout mode to transition to.
     */
    public void changeLayoutTo(int mode) {
        assert(mode != INVALID_LAYOUT);
        if (mode == mCurrentLayout || mode == INVALID_LAYOUT) return;

        mCurrentLayout = mode;
        removeCallbacks(mDismissSteadyLayoutRunnable);
        setVisibilityForEditLayouts(false);
        if (mode == LAYOUT_FETCHING) {
            mSteadyLayout.setVisibility(GONE);
            findViewById(R.id.loading_icon).setVisibility(VISIBLE);
            return;
        }
        findViewById(R.id.loading_icon).setVisibility(GONE);
        mSteadyLayout.setVisibility(VISIBLE);
        if (mode == LAYOUT_STEADY) return;

        addTranslateAnimations(mode);
        addDisappearAnimationForSteadyLayout();
        View centeredLayout = mEditLayouts[getSectionForLayoutMode(mode)];
        addAppearAnimationForEditLayout(mode, centeredLayout);

        centeredLayout.setVisibility(VISIBLE);
        if (mode == LAYOUT_EDITING_CC_BILLING) {
            mEditLayouts[SECTION_CC].setVisibility(VISIBLE);
            mEditLayouts[SECTION_BILLING].setVisibility(VISIBLE);
        }
        ((View) centeredLayout.getParent()).setVisibility(VISIBLE);
        centeredLayout.animate();
        mSteadyLayout.animate();
        postDelayed(mDismissSteadyLayoutRunnable, ANIMATION_DURATION_MS);

        mCurrentLayout = mode;
    }

    private void setVisibilityForEditLayouts(boolean visible) {
        int visibility = visible ? VISIBLE : GONE;
        for (int i = 0; i < NUM_SECTIONS; i++) {
            if (mEditLayouts[i] != null) mEditLayouts[i].setVisibility(visibility);
        }
    }

    /**
     * Sets the visibility for all items related with the given section.
     * @param section The section that will change visibility.
     * @param visible Whether the section should be visible.
     */
    public void setVisibilityForSection(int section, boolean visible) {
        int visibility = visible ? VISIBLE : GONE;
        mSpinners[section].setVisibility(visibility);
        View labelView = findViewById(AutofillDialogUtils.getLabelIDForSection(section));
        if (labelView != null) labelView.setVisibility(visibility);
    }

    private void addAppearAnimationForEditLayout(int mode, View layout) {
        View centerView = mSpinners[getSectionForLayoutMode(mode)];
        float yOffset = centerView.getY() - (float) centerView.getHeight() / 2;

        TranslateAnimation toLocationAnimation = new TranslateAnimation(0, 0, yOffset, 0);
        toLocationAnimation.setDuration(ANIMATION_DURATION_MS);
        ScaleAnimation scaleAnimation = new ScaleAnimation(1, 1, 0, 1,
                Animation.RELATIVE_TO_SELF, 0, Animation.RELATIVE_TO_SELF, 0.5f);
        scaleAnimation.setDuration(ANIMATION_DURATION_MS);
        scaleAnimation.setStartOffset(ANIMATION_DURATION_MS / 4);

        AnimationSet appearAnimation = new AnimationSet(true);
        appearAnimation.addAnimation(toLocationAnimation);
        appearAnimation.addAnimation(scaleAnimation);

        layout.setAnimation(appearAnimation);
    }

    private void addDisappearAnimationForSteadyLayout() {
        ScaleAnimation scaleDownAnimation = new ScaleAnimation(1, 1, 1, 0,
                Animation.RELATIVE_TO_SELF, 0, Animation.RELATIVE_TO_SELF, 0.5f);
        scaleDownAnimation.setDuration(ANIMATION_DURATION_MS / 2);
        scaleDownAnimation.setStartOffset(ANIMATION_DURATION_MS / 2);

        mSteadyLayout.setAnimation(scaleDownAnimation);
    }

    private void addTranslateAnimations(int mode) {
        float distance = getResources().
                getDimensionPixelSize(R.dimen.autofill_translation_anim_distance);
        TranslateAnimation toTopAnimation = new TranslateAnimation(0, 0, 0, -distance);
        toTopAnimation.setDuration(ANIMATION_DURATION_MS);
        TranslateAnimation toBottomAnimation = new TranslateAnimation(0, 0, 0, distance);
        toBottomAnimation.setDuration(ANIMATION_DURATION_MS);
        for (int i = 0; i < mSteadyLayout.getChildCount(); i++) {
            View currentChild = mSteadyLayout.getChildAt(i);
            if (currentChild.getVisibility() == GONE) continue;

            if (currentChild.getTop() <=
                    mSpinners[getSectionForLayoutMode(mode)].getTop()) {
                currentChild.setAnimation(toTopAnimation);
                currentChild.animate();
            } else if (currentChild.getTop() >
                    mSpinners[getSectionForLayoutMode(mode)].getTop()) {
                currentChild.setAnimation(toBottomAnimation);
                currentChild.animate();
            }
        }
    }

    private static int getSectionForLayoutMode(int mode) {
        switch (mode) {
            case LAYOUT_EDITING_CC:
                return SECTION_CC;
            case LAYOUT_EDITING_BILLING:
                return SECTION_BILLING;
            case LAYOUT_EDITING_CC_BILLING:
                return SECTION_CC_BILLING;
            case LAYOUT_EDITING_SHIPPING:
                return SECTION_SHIPPING;
            default:
                assert(false);
                return AutofillDialogUtils.INVALID_SECTION;
        }
    }

    /**
     * Returns the layout mode for editing the given section.
     * @param section The section to look up.
     * @return The layout mode for editing the given section.
     */
    public static int getLayoutModeForSection(int section) {
        assert(section != AutofillDialogUtils.INVALID_SECTION);
        for (int i = 0; i < AutofillDialogConstants.NUM_SECTIONS; i++) {
            if (getSectionForLayoutMode(i) == section) return i;
        }
        assert(false);
        return INVALID_LAYOUT;
    }

    private static class AutofillDialogMenuAdapter extends ArrayAdapter<AutofillDialogMenuItem> {

        public AutofillDialogMenuAdapter(Context context, List<AutofillDialogMenuItem> objects) {
            super(context, R.layout.autofill_menu_item, objects);
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            return initView(position, convertView);
        }

        @Override
        public View getDropDownView(int position, View convertView, ViewGroup parent) {
            return initView(position, convertView);
        }

        private View initView(int position, View convertView) {
            if (convertView == null) {
                convertView = View.inflate(getContext(),
                        R.layout.autofill_menu_item, null);
            }
            AutofillDialogMenuItem item = getItem(position);
            ImageView icon = (ImageView) convertView.findViewById(R.id.cc_icon);
            TextView line1 = (TextView) convertView.findViewById(R.id.adapter_item_line_1);
            TextView line2 = (TextView) convertView.findViewById(R.id.adapter_item_line_2);
            if (icon != null) {
                if (item.mIcon != null) {
                    icon.setImageBitmap(item.mIcon);
                    icon.setVisibility(VISIBLE);
                } else {
                    icon.setImageBitmap(null);
                    icon.setVisibility(GONE);
                }
            }
            if (line1 != null) line1.setText(item.mLine1);
            if (line2 != null) {
                if (!TextUtils.isEmpty(item.mLine2)) {
                    line2.setVisibility(VISIBLE);
                    line2.setText(item.mLine2);
                } else {
                    line2.setVisibility(GONE);
                }
            }
            return convertView;
        }
    }
}
