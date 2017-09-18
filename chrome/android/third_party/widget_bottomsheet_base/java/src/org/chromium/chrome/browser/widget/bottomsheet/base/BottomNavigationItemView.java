/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.chromium.chrome.browser.widget.bottomsheet.base;

import static android.support.annotation.RestrictTo.Scope.GROUP_ID;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.support.annotation.NonNull;
import android.support.annotation.RestrictTo;
import android.support.design.R;
import android.support.v4.content.ContextCompat;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v4.view.ViewCompat;
import android.support.v7.view.menu.MenuItemImpl;
import android.support.v7.view.menu.MenuView;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * Forked from android.support.design.internal.BottomNavigationItemView.
 *
 * @hide
 */
@RestrictTo(GROUP_ID)
public class BottomNavigationItemView extends FrameLayout implements MenuView.ItemView {
    public static final int INVALID_ITEM_POSITION = -1;

    private static final int[] CHECKED_STATE_SET = {android.R.attr.state_checked};

    private final int mDefaultMargin;
    private final float mScaleUpFactor;
    private final float mScaleDownFactor;

    private ImageView mIcon;
    private final TextView mSmallLabel;
    private final TextView mLargeLabel;
    private int mItemPosition = INVALID_ITEM_POSITION;
    private boolean mLabelHidden;

    private MenuItemImpl mItemData;

    private ColorStateList mIconTint;

    public BottomNavigationItemView(@NonNull Context context) {
        this(context, null);
    }

    public BottomNavigationItemView(@NonNull Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public BottomNavigationItemView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        final Resources res = getResources();
        int inactiveLabelSize =
                res.getDimensionPixelSize(R.dimen.design_bottom_navigation_text_size);
        int activeLabelSize =
                res.getDimensionPixelSize(R.dimen.design_bottom_navigation_active_text_size);
        mDefaultMargin = res.getDimensionPixelSize(R.dimen.design_bottom_navigation_margin);
        mScaleUpFactor = 1f * activeLabelSize / inactiveLabelSize;
        mScaleDownFactor = 1f * inactiveLabelSize / activeLabelSize;

        LayoutInflater.from(context).inflate(R.layout.design_bottom_navigation_item, this, true);
        setBackgroundResource(R.drawable.design_bottom_navigation_item_background);
        mIcon = (ImageView) findViewById(R.id.icon);
        mSmallLabel = (TextView) findViewById(R.id.smallLabel);
        mLargeLabel = (TextView) findViewById(R.id.largeLabel);
    }

    @Override
    public void initialize(MenuItemImpl itemData, int menuType) {
        mItemData = itemData;
        setCheckable(itemData.isCheckable());
        setChecked(itemData.isChecked());
        setEnabled(itemData.isEnabled());
        setIcon(itemData.getIcon());
        setTitle(itemData.getTitle());
        setId(itemData.getItemId());
    }

    public void setItemPosition(int position) {
        mItemPosition = position;
    }

    public int getItemPosition() {
        return mItemPosition;
    }

    @Override
    public MenuItemImpl getItemData() {
        return mItemData;
    }

    @Override
    public void setTitle(CharSequence title) {
        mSmallLabel.setText(title);
        mLargeLabel.setText(title);
    }

    @Override
    public void setCheckable(boolean checkable) {
        refreshDrawableState();
    }

    @Override
    public void setChecked(boolean checked) {
        LayoutParams iconParams = (LayoutParams) mIcon.getLayoutParams();
        iconParams.gravity = Gravity.CENTER_HORIZONTAL | Gravity.CENTER;
        iconParams.topMargin = mDefaultMargin;
        mIcon.setLayoutParams(iconParams);

        if (!mLabelHidden) {
            ViewCompat.setPivotX(mLargeLabel, mLargeLabel.getWidth() / 2f);
            ViewCompat.setPivotY(mLargeLabel, mLargeLabel.getBaseline());
            ViewCompat.setPivotX(mSmallLabel, mSmallLabel.getWidth() / 2f);
            ViewCompat.setPivotY(mSmallLabel, mSmallLabel.getBaseline());

            if (checked) {
                mLargeLabel.setVisibility(VISIBLE);
                mSmallLabel.setVisibility(INVISIBLE);

                ViewCompat.setScaleX(mLargeLabel, 1f);
                ViewCompat.setScaleY(mLargeLabel, 1f);
                ViewCompat.setScaleX(mSmallLabel, mScaleUpFactor);
                ViewCompat.setScaleY(mSmallLabel, mScaleUpFactor);
            } else {
                mLargeLabel.setVisibility(INVISIBLE);
                mSmallLabel.setVisibility(VISIBLE);

                ViewCompat.setScaleX(mLargeLabel, mScaleDownFactor);
                ViewCompat.setScaleY(mLargeLabel, mScaleDownFactor);
                ViewCompat.setScaleX(mSmallLabel, 1f);
                ViewCompat.setScaleY(mSmallLabel, 1f);
            }
        }

        refreshDrawableState();
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        mSmallLabel.setEnabled(enabled);
        mLargeLabel.setEnabled(enabled);
        mIcon.setEnabled(enabled);
    }

    @Override
    public int[] onCreateDrawableState(final int extraSpace) {
        final int[] drawableState = super.onCreateDrawableState(extraSpace + 1);
        if (mItemData != null && mItemData.isCheckable() && mItemData.isChecked()) {
            mergeDrawableStates(drawableState, CHECKED_STATE_SET);
        }
        return drawableState;
    }

    @Override
    public void setShortcut(boolean showShortcut, char shortcutKey) {}

    @Override
    public void setIcon(Drawable icon) {
        if (icon != null) {
            Drawable.ConstantState state = icon.getConstantState();
            icon = DrawableCompat.wrap(state == null ? icon : state.newDrawable()).mutate();
            DrawableCompat.setTintList(icon, mIconTint);
        }
        mIcon.setImageDrawable(icon);
    }

    @Override
    public boolean prefersCondensedTitle() {
        return false;
    }

    @Override
    public boolean showsIcon() {
        return true;
    }

    /**
     * Hides the label below the menu item's icon. Also adds a content description for accessibility
     * purposes since there's no label to read any more.
     */
    public void hideLabel() {
        mLabelHidden = true;
        mSmallLabel.setVisibility(GONE);
        mLargeLabel.setVisibility(GONE);
        setContentDescription(mItemData.getTitle());
    }

    public void setIconTintList(ColorStateList tint) {
        mIconTint = tint;
        if (mItemData != null) {
            // Update the icon so that the tint takes effect
            setIcon(mItemData.getIcon());
        }
    }

    public void setTextColor(ColorStateList color) {
        mSmallLabel.setTextColor(color);
        mLargeLabel.setTextColor(color);
    }

    public void setItemBackground(int background) {
        Drawable backgroundDrawable =
                background == 0 ? null : ContextCompat.getDrawable(getContext(), background);
        ViewCompat.setBackground(this, backgroundDrawable);
    }
}
