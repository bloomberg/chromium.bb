// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.selection;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.widget.Checkable;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.widget.TintedImageView;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate.SelectionObserver;

import java.util.List;

/**
 * An item that can be selected. When selected, the item will be highlighted. A selection is
 * initially established via long-press. If a selection is already established, clicking on the item
 * will toggle its selection.
 *
 * @param <E> The type of the item associated with this SelectableItemView.
 */
public abstract class SelectableItemView<E> extends FrameLayout implements Checkable,
        OnClickListener, OnLongClickListener, SelectionObserver<E> {
    protected final int mDefaultLevel;
    protected final int mSelectedLevel;

    protected TintedImageView mIconView;
    protected TextView mTitleView;
    protected TextView mDescriptionView;
    protected Drawable mIconDrawable;
    protected ColorStateList mIconColorList;

    private SelectionDelegate<E> mSelectionDelegate;
    private SelectableItemHighlightView mHighlightView;
    private E mItem;

    /**
     * Constructor for inflating from XML.
     */
    public SelectableItemView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mIconColorList =
                ApiCompatibilityUtils.getColorStateList(getResources(), R.color.white_mode_tint);
        mDefaultLevel = getResources().getInteger(R.integer.selectable_item_level_default);
        mSelectedLevel = getResources().getInteger(R.integer.selectable_item_level_selected);
    }

    /**
     * Destroys and cleans up itself.
     */
    public void destroy() {
        if (mSelectionDelegate != null) {
            mSelectionDelegate.removeObserver(this);
        }
    }

    /**
     * Sets the SelectionDelegate and registers this object as an observer. The SelectionDelegate
     * must be set before the item can respond to click events.
     * @param delegate The SelectionDelegate that will inform this item of selection changes.
     */
    public void setSelectionDelegate(SelectionDelegate<E> delegate) {
        if (mSelectionDelegate != delegate) {
            if (mSelectionDelegate != null) mSelectionDelegate.removeObserver(this);
            mSelectionDelegate = delegate;
            mSelectionDelegate.addObserver(this);
        }
    }

    /**
     * @param item The item associated with this SelectableItemView.
     */
    public void setItem(E item) {
        mItem = item;
        setChecked(mSelectionDelegate.isItemSelected(item));
    }

    /**
     * @return The item associated with this SelectableItemView.
     */
    public E getItem() {
        return mItem;
    }

    // FrameLayout implementations.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        inflate(getContext(), R.layout.selectable_item_highlight_view, this);
        mHighlightView = (SelectableItemHighlightView) findViewById(R.id.highlight);
        mIconView = findViewById(R.id.icon_view);
        mTitleView = findViewById(R.id.title);
        mDescriptionView = findViewById(R.id.description);

        if (mIconView != null) {
            if (FeatureUtilities.isChromeHomeModernEnabled()) {
                mIconView.setBackgroundResource(R.drawable.selectable_item_icon_modern_bg);
            }
            mIconView.setTint(null);
        }

        setOnClickListener(this);
        setOnLongClickListener(this);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (mSelectionDelegate != null) {
            setChecked(mSelectionDelegate.isItemSelected(mItem));
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        setChecked(false);
    }

    // OnClickListener implementation.
    @Override
    public final void onClick(View view) {
        assert view == this;

        if (isSelectionModeActive()) {
            onLongClick(view);
        }  else {
            onClick();
        }
    }

    // OnLongClickListener implementation.
    @Override
    public boolean onLongClick(View view) {
        assert view == this;
        boolean checked = toggleSelectionForItem(mItem);
        setChecked(checked);
        return true;
    }

    /**
     * @return Whether we are currently in selection mode.
     */
    protected boolean isSelectionModeActive() {
        return mSelectionDelegate.isSelectionEnabled();
    }

    /**
     * Toggles the selection state for a given item.
     * @param item The given item.
     * @return Whether the item was in selected state after the toggle.
     */
    protected boolean toggleSelectionForItem(E item) {
        return mSelectionDelegate.toggleSelectionForItem(item);
    }

    // Checkable implementations.
    @Override
    public boolean isChecked() {
        return mHighlightView.isChecked();
    }

    @Override
    public void toggle() {
        setChecked(!isChecked());
    }

    @Override
    public void setChecked(boolean checked) {
        mHighlightView.setChecked(checked);
        updateIconView();
    }

    // SelectionObserver implementation.
    @Override
    public void onSelectionStateChange(List<E> selectedItems) {
        setChecked(mSelectionDelegate.isItemSelected(mItem));
    }

    /**
     * Update cached icon drawable when icon view's drawable is changed. Note that this method must
     * be called after the drawable is changed to ensure that it can be set back from the check
     * icon in selection mode.
     */
    protected void onIconDrawableChanged() {
        mIconDrawable = mIconView.getDrawable();
    }

    /**
     * Update icon image and background based on whether this item is selected.
     */
    protected void updateIconView() {
        // TODO(huayinz): Refactor this method so that mIconView is not exposed to subclass.
        if (mIconView == null) return;

        if (FeatureUtilities.isChromeHomeModernEnabled()) {
            if (isChecked()) {
                mIconView.getBackground().setLevel(mSelectedLevel);
                mIconView.setImageResource(R.drawable.ic_check_googblue_24dp);
                mIconView.setTint(mIconColorList);
            } else {
                mIconView.getBackground().setLevel(mDefaultLevel);
                mIconView.setImageDrawable(mIconDrawable);
                mIconView.setTint(null);
            }
        }
    }

    /**
     * Same as {@link OnClickListener#onClick(View)} on this.
     * Subclasses should override this instead of setting their own OnClickListener because this
     * class handles onClick events in selection mode, and won't forward events to subclasses in
     * that case.
     */
    protected abstract void onClick();
}
