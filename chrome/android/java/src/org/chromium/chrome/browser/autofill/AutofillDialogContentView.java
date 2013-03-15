// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import java.util.ArrayList;
import java.util.List;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.view.animation.AnimationSet;
import android.view.animation.ScaleAnimation;
import android.view.animation.TranslateAnimation;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.AdapterView.OnItemSelectedListener;

import static org.chromium.chrome.browser.autofill.AutofillDialogConstants.NUM_SECTIONS;;
import static org.chromium.chrome.browser.autofill.AutofillDialogConstants.SECTION_CC_BILLING;
import static org.chromium.chrome.browser.autofill.AutofillDialogConstants.SECTION_SHIPPING;

import org.chromium.chrome.R;

/**
 * This is the parent layout that contains the layouts for different states of
 * autofill dialog. In principle it shouldn't contain any logic related with the
 * actual workflow, but rather respond to any UI update messages coming to it
 * from the AutofillDialog.
 */
public class AutofillDialogContentView extends FrameLayout {
    private static final int ANIMATION_DURATION_MS = 1000;
    // TODO(yusufo): Remove all placeholders here and also in related layout xml files.
    private final AutofillDialogMenuItem[][] mDefaultMenuItems =
            new AutofillDialogMenuItem[NUM_SECTIONS][];
    static final int LAYOUT_FETCHING = 0;
    static final int LAYOUT_STEADY = 1;
    static final int LAYOUT_EDITING_SHIPPING = 2;
    static final int LAYOUT_EDITING_BILLING = 3;

    private final ArrayList<View> mTopViewGroup;
    private final ArrayList<View> mMidViewGroup;
    private final ArrayList<View> mBottomViewGroup;
    private final Runnable mDismissSteadyLayoutRunnable = new Runnable() {
        @Override
        public void run() {
            mSteadyLayout.setVisibility(GONE);
        }
    };
    private Spinner[] mSpinners = new Spinner[NUM_SECTIONS];
    private AutofillDialogMenuAdapter[] mAdapters = new AutofillDialogMenuAdapter[NUM_SECTIONS];
    private View mSteadyLayout;
    private View mEditShippingLayout;
    private View mEditBillingLayout;
    private int mCurrentLayout = -1;

    public AutofillDialogContentView(Context context, AttributeSet attrs) {
        super(context, attrs);
        AutofillDialogMenuItem[] billingItems = {
                new AutofillDialogMenuItem(0,
                        getResources().getString(R.string.autofill_new_billing)),
                new AutofillDialogMenuItem(0,
                        getResources().getString(R.string.autofill_edit_billing))
        };
        AutofillDialogMenuItem[] shippingItems = {
                new AutofillDialogMenuItem(0,
                        getResources().getString(R.string.autofill_new_shipping)),
                new AutofillDialogMenuItem(0,
                        getResources().getString(R.string.autofill_edit_shipping))
        };

        mDefaultMenuItems[SECTION_CC_BILLING] = billingItems;
        mDefaultMenuItems[SECTION_SHIPPING] = shippingItems;
        mTopViewGroup = new ArrayList<View>();
        mMidViewGroup = new ArrayList<View>();
        mBottomViewGroup = new ArrayList<View>();
    }

    private void setViewGroups() {
        mTopViewGroup.clear();
        mTopViewGroup.add(findViewById(R.id.billing_title));
        mTopViewGroup.add(findViewById(R.id.cc_spinner));

        mMidViewGroup.clear();
        mMidViewGroup.add(findViewById(R.id.shipping_title));
        mMidViewGroup.add(findViewById(R.id.address_spinner));

        mBottomViewGroup.clear();
        mBottomViewGroup.add(findViewById(R.id.check_box));
        mBottomViewGroup.add(findViewById(R.id.line_bottom));
        mBottomViewGroup.add(findViewById(R.id.terms_info));
    }

    @Override
    protected void onFinishInflate () {
        mSteadyLayout = findViewById(R.id.general_layout);
        mEditBillingLayout = findViewById(R.id.editing_layout_billing);
        mEditShippingLayout = findViewById(R.id.editing_layout_shipping);

        for (int i = 0; i < AutofillDialogConstants.NUM_SECTIONS; i++) {
            int id = AutofillDialogUtils.getSpinnerIDForSection(i);
            mSpinners[i] = (Spinner) findViewById(id);
            AutofillDialogMenuAdapter adapter = new AutofillDialogMenuAdapter(getContext(),
                    new ArrayList<AutofillDialogMenuItem>());
            mAdapters[i] = adapter;
            if (id == AutofillDialogUtils.INVALID_ID) continue;
            mSpinners[i].setAdapter(adapter);
        }

        setViewGroups();
        createAndAddPlaceHolders();
        changeLayoutTo(LAYOUT_FETCHING);
    }

    // TODO(yusufo): Remove this method once glue implements fetching data.
    private void createAndAddPlaceHolders() {
        AutofillDialogMenuItem[] ccItems = new AutofillDialogMenuItem[1];
        ccItems[0] = new AutofillDialogMenuItem(
                0, "XXXX-XXXX-XXXX-1000", "Additional info required", null);
        AutofillDialogMenuItem[] addressItems = new AutofillDialogMenuItem[1];
        addressItems[0] = new AutofillDialogMenuItem(
                0, "Place Holder", "1600 Amphitheatre Pkwy", null);
        updateMenuItemsForSection(SECTION_CC_BILLING, ccItems);
        updateMenuItemsForSection(SECTION_SHIPPING, addressItems);
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
        mSpinners[SECTION_CC_BILLING].setOnItemSelectedListener(listener);
        mSpinners[SECTION_SHIPPING].setOnItemSelectedListener(listener);
    }

    /**
     * @param spinner The dropdown that was selected by the user.
     * @param position The position for the selected item in the dropdown.
     * @return Whether the selection should cause a layout state change.
     */
    public boolean selectionShouldChangeLayout(AdapterView<?> spinner, int position) {
        int numConstantItems = spinner.getId() == R.id.cc_spinner ?
                mDefaultMenuItems[SECTION_CC_BILLING].length :
                mDefaultMenuItems[SECTION_SHIPPING].length;
        return position >= spinner.getCount() - numConstantItems;
    }

    /**
     * @return The current layout the content is showing.
     */
    // TODO(yusufo): Consider restricting this access more to checks rather than the
    // current value.
    public int getCurrentLayout() {
        return mCurrentLayout;
    }

    /**
     * Updates a dropdown with the given items and adds default items to the end.
     * @param items The {@link AutofillDialogMenuItem} array to update the dropdown with.
     */
    public void updateMenuItemsForSection(int section, AutofillDialogMenuItem[] items) {
        AutofillDialogMenuAdapter adapter = mAdapters[section];
        adapter.clear();
        adapter.addAll(items);
        if (mDefaultMenuItems[section] != null) adapter.addAll(mDefaultMenuItems[section]);
    }

    /**
     * Transitions the layout shown to a given layout.
     * @param mode The layout mode to transition to.
     */
    public void changeLayoutTo(int mode) {
        if (mode == mCurrentLayout) return;

        mCurrentLayout = mode;
        removeCallbacks(mDismissSteadyLayoutRunnable);
        mEditBillingLayout.setVisibility(GONE);
        mEditShippingLayout.setVisibility(GONE);
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
        View centeredLayout = mode == LAYOUT_EDITING_BILLING ?
                mEditBillingLayout : mEditShippingLayout;
        addAppearAnimationForEditLayout(mode, centeredLayout);

        centeredLayout.setVisibility(VISIBLE);
        centeredLayout.animate();
        mSteadyLayout.animate();
        postDelayed(mDismissSteadyLayoutRunnable, ANIMATION_DURATION_MS);

        mCurrentLayout = mode;
    }

    private void addAppearAnimationForEditLayout(int mode, View layout) {
        View centerView = mode == LAYOUT_EDITING_BILLING ?
                mSpinners[SECTION_CC_BILLING] : mSpinners[SECTION_SHIPPING];
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
        TranslateAnimation midViewGroupAnimation = mode == LAYOUT_EDITING_BILLING ?
                toBottomAnimation : toTopAnimation;

        for (int i = 0; i < mTopViewGroup.size(); i++) {
            mTopViewGroup.get(i).setAnimation(toTopAnimation);
            mTopViewGroup.get(i).animate();
        }
        for (int i = 0; i < mMidViewGroup.size(); i++) {
            mMidViewGroup.get(i).setAnimation(midViewGroupAnimation);
            mMidViewGroup.get(i).animate();
        }
        for (int i = 0; i < mBottomViewGroup.size(); i++) {
            mBottomViewGroup.get(i).setAnimation(toBottomAnimation);
            mBottomViewGroup.get(i).animate();
        }
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
            // TODO(yusufo): Fix card icon when it gets added to menuItem.
            if (icon != null) icon.setImageResource(R.drawable.visa);
            if (line1 != null) line1.setText(item.mLine1);
            if (line2 != null) line2.setText(item.mLine2);
            return convertView;
        }
    }
}
