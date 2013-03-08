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
    private final List<AddressItem> mDefaultShippingItems =
            new ArrayList<AddressItem>();
    private final List<CreditCardItem> mDefaultBillingItems =
            new ArrayList<CreditCardItem>();
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
    private Spinner mCreditCardSpinner;
    private CreditCardAdapter mCreditCardAdapter;
    private Spinner mAddressSpinner;
    private AddressAdapter mAddressAdapter;
    private View mSteadyLayout;
    private View mEditShippingLayout;
    private View mEditBillingLayout;
    private int mCurrentLayout = -1;

    public AutofillDialogContentView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mDefaultShippingItems.add(new AddressItem(
                getResources().getString(R.string.autofill_new_shipping), ""));
        mDefaultShippingItems.add(new AddressItem(
                getResources().getString(R.string.autofill_edit_shipping), ""));
        mDefaultBillingItems.add(new CreditCardItem(0,
                getResources().getString(R.string.autofill_new_billing), ""));
        mDefaultBillingItems.add(new CreditCardItem(0,
                getResources().getString(R.string.autofill_edit_billing), ""));
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

        mCreditCardSpinner = (Spinner) findViewById(R.id.cc_spinner);
        List<CreditCardItem> cards = new ArrayList<CreditCardItem>();
        mCreditCardAdapter = new CreditCardAdapter(getContext(), cards);
        mCreditCardSpinner.setAdapter(mCreditCardAdapter);

        List<AddressItem> addresses = new ArrayList<AddressItem>();
        mAddressSpinner = (Spinner) findViewById(R.id.address_spinner);
        mAddressAdapter = new AddressAdapter(getContext(), addresses);
        mAddressSpinner.setAdapter(mAddressAdapter);

        setViewGroups();
        createAndAddPlaceHolders();
        changeLayoutTo(LAYOUT_FETCHING);
    }

    // TODO(yusufo): The AddressItem and CreditCardItem object will be created in the content view
    // using data fetched from natuve side, but ANdroid side will be the the only platform using
    // them. Remove this method or replace it with one that uses the fetched data.
    private void createAndAddPlaceHolders() {
        mCreditCardAdapter.add(new CreditCardItem(R.drawable.visa,
                "XXXX-XXXX-XXXX-1000", "Additional info required"));
        mCreditCardAdapter.add(new CreditCardItem(R.drawable.master_card,
                "XXXX-XXXX-XXXX-1000", ""));
        mCreditCardAdapter.addAll(mDefaultBillingItems);
        mAddressAdapter.add(new AddressItem("Place Holder", "1600 Amphitheatre Pkwy"));
        mAddressAdapter.addAll(mDefaultShippingItems);
    }

    @Override
    protected void onMeasure (int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        mCreditCardSpinner.setDropDownWidth(mCreditCardSpinner.getMeasuredWidth());
        mCreditCardSpinner.setDropDownVerticalOffset(-mCreditCardSpinner.getMeasuredHeight());
        mAddressSpinner.setDropDownWidth(mAddressSpinner.getMeasuredWidth());
        mAddressSpinner.setDropDownVerticalOffset(-mAddressSpinner.getMeasuredHeight());
    }

    /**
     * Set the listener for all the dropdown members in the layout.
     * @param listener The listener object to attach to the dropdowns.
     */
    public void setOnItemSelectedListener(OnItemSelectedListener listener) {
        mCreditCardSpinner.setOnItemSelectedListener(listener);
        mAddressSpinner.setOnItemSelectedListener(listener);
    }

    /**
     * @param spinner The dropdown that was selected by the user.
     * @param position The position for the selected item in the dropdown.
     * @return Whether the selection should cause a layout state change.
     */
    public boolean selectionShouldChangeLayout(AdapterView<?> spinner, int position) {
        int numConstantItems = spinner.getId() == R.id.cc_spinner ?
                mDefaultBillingItems.size() : mDefaultShippingItems.size();
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
                mCreditCardSpinner : mAddressSpinner;
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

    private static class AddressItem {
        private final String mName;
        private final String mAddress;

        public AddressItem(String name, String address) {
            mName = name;
            mAddress = address;
        }
    }

    private static class AddressAdapter extends ArrayAdapter<AddressItem> {

        public AddressAdapter(Context context, List<AddressItem> objects) {
            super(context, R.layout.autofill_address_spinner_dropdown, objects);
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
                        R.layout.autofill_address_spinner_dropdown, null);
            }
            AddressItem item = getItem(position);
            TextView nameView = (TextView)convertView.findViewById(R.id.name);
            TextView addressView = (TextView)convertView.findViewById(R.id.address);
            if (nameView != null) nameView.setText(item.mName);
            if (addressView != null) addressView.setText(item.mAddress);
            return convertView;
        }
    }

    private static class CreditCardItem {
        private final int mCardIconID;
        private final String mLastDigits;
        private final String mInfo;

        public CreditCardItem(int icon, String lastDigits, String info) {
            mCardIconID = icon;
            mLastDigits = lastDigits;
            mInfo = info;
        }
    }

    private static class CreditCardAdapter extends ArrayAdapter<CreditCardItem> {

        public CreditCardAdapter(Context context, List<CreditCardItem> objects) {
            super(context, R.layout.autofill_cc_spinner_dropdown, objects);
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
                        R.layout.autofill_cc_spinner_dropdown, null);
            }
            CreditCardItem item = getItem(position);
            ImageView icon = (ImageView)convertView.findViewById(R.id.cc_icon);
            TextView lastDigitsView = (TextView)convertView.findViewById(R.id.cc_number);
            TextView infoView = (TextView)convertView.findViewById(R.id.cc_info);
            if (icon != null) icon.setImageResource(item.mCardIconID);
            if (lastDigitsView != null) lastDigitsView.setText(item.mLastDigits);
            if (infoView != null) infoView.setText(item.mInfo);
            return convertView;
        }
    }
}
