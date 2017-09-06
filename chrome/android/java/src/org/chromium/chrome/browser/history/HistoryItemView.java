// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.chrome.browser.widget.selection.SelectableItemView;

/**
 * The SelectableItemView for items displayed in the browsing history UI.
 */
public class HistoryItemView extends SelectableItemView<HistoryItem> implements LargeIconCallback {
    private TintedImageButton mRemoveButton;
    private VectorDrawableCompat mBlockedVisitDrawable;
    private View mContentView;

    private HistoryManager mHistoryManager;
    private final RoundedIconGenerator mIconGenerator;

    private final int mMinIconSize;
    private final int mDisplayedIconSize;
    private final int mCornerRadius;
    private final int mEndPadding;

    private boolean mRemoveButtonVisible;
    private boolean mIsItemRemoved;

    public HistoryItemView(Context context, AttributeSet attrs) {
        super(context, attrs);

        mCornerRadius = getResources().getDimensionPixelSize(R.dimen.default_favicon_corner_radius);
        mMinIconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mDisplayedIconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        int textSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_icon_text_size);
        int iconColor = ApiCompatibilityUtils.getColor(
                getResources(), R.color.default_favicon_background_color);
        mIconGenerator = new RoundedIconGenerator(mDisplayedIconSize, mDisplayedIconSize,
                FeatureUtilities.isChromeHomeModernEnabled() ? mDisplayedIconSize / 2
                                                             : mCornerRadius,
                iconColor, textSize);
        mEndPadding = context.getResources().getDimensionPixelSize(
                R.dimen.selectable_list_layout_row_padding);

        mIconColorList = ApiCompatibilityUtils.getColorStateList(
                context.getResources(), R.color.white_mode_tint);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIconView.setImageResource(R.drawable.default_favicon);
        mContentView = findViewById(R.id.content);
        mRemoveButton = (TintedImageButton) findViewById(R.id.remove);
        mRemoveButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                remove();
            }
        });

        updateRemoveButtonVisibility();
    }

    @Override
    public void setItem(HistoryItem item) {
        if (getItem() == item) return;

        super.setItem(item);

        mTitleView.setText(item.getTitle());
        mDescriptionView.setText(item.getDomain());
        mIsItemRemoved = false;

        if (item.wasBlockedVisit()) {
            if (mBlockedVisitDrawable == null) {
                mBlockedVisitDrawable = VectorDrawableCompat.create(
                        getContext().getResources(), R.drawable.ic_block_red,
                        getContext().getTheme());
            }
            setIconDrawable(mBlockedVisitDrawable);
            mTitleView.setTextColor(
                    ApiCompatibilityUtils.getColor(getResources(), R.color.google_red_700));
        } else {
            setIconDrawable(
                    ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.default_favicon));
            if (mHistoryManager != null) requestIcon();

            mTitleView.setTextColor(
                    ApiCompatibilityUtils.getColor(getResources(), R.color.default_text_color));
        }
    }

    /**
     * @param manager The HistoryManager associated with this item.
     */
    public void setHistoryManager(HistoryManager manager) {
        getItem().setHistoryManager(manager);
        if (mHistoryManager == manager) return;

        mHistoryManager = manager;
        if (!getItem().wasBlockedVisit()) requestIcon();
    }

    /**
     * Removes the item associated with this view.
     */
    public void remove() {
        // If the remove button is double tapped, this method may be called twice.
        if (getItem() == null || mIsItemRemoved) return;

        mIsItemRemoved = true;
        getItem().remove();
    }

    /**
     * Should be called when the user's sign in state changes.
     */
    public void onSignInStateChange() {
        updateRemoveButtonVisibility();
    }

    /**
     * @param visible Whether the remove button should be visible. Note that this method will have
     *                no effect if the button is GONE because the signed in user is not allowed to
     *                delete browsing history.
     */
    public void setRemoveButtonVisible(boolean visible) {
        mRemoveButtonVisible = visible;
        if (!PrefServiceBridge.getInstance().canDeleteBrowsingHistory()) return;

        mRemoveButton.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
    }

    @Override
    protected void onClick() {
        if (getItem() != null) getItem().open();
    }

    @Override
    public void onLargeIconAvailable(Bitmap icon, int fallbackColor,
            boolean isFallbackColorDefault) {
        // TODO(twellington): move this somewhere that can be shared with bookmarks.
        if (icon == null) {
            mIconGenerator.setBackgroundColor(fallbackColor);
            icon = mIconGenerator.generateIconForUrl(getItem().getUrl());
            setIconDrawable(new BitmapDrawable(getResources(), icon));
        } else {
            RoundedBitmapDrawable roundedIcon = RoundedBitmapDrawableFactory.create(
                    getResources(),
                    Bitmap.createScaledBitmap(icon, mDisplayedIconSize, mDisplayedIconSize, false));
            roundedIcon.setCornerRadius(mCornerRadius);
            setIconDrawable(roundedIcon);
        }
    }

    private void requestIcon() {
        if (mHistoryManager == null || mHistoryManager.getLargeIconBridge() == null) return;

        mHistoryManager.getLargeIconBridge().getLargeIconForUrl(
                getItem().getUrl(), mMinIconSize, this);
    }

    private void updateRemoveButtonVisibility() {
        int removeButtonVisibility =
                !PrefServiceBridge.getInstance().canDeleteBrowsingHistory() ? View.GONE
                        : mRemoveButtonVisible ? View.VISIBLE : View.INVISIBLE;
        mRemoveButton.setVisibility(removeButtonVisibility);

        int endPadding = removeButtonVisibility == View.GONE ? mEndPadding : 0;
        ApiCompatibilityUtils.setPaddingRelative(mContentView,
                ApiCompatibilityUtils.getPaddingStart(mContentView),
                mContentView.getPaddingTop(), endPadding, mContentView.getPaddingBottom());
    }
}
