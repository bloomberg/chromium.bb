// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.PopupWindow;

/**
 * Paste popup implementation based on TextView.PastePopupMenu.
 */
public class LegacyPastePopupMenu implements OnClickListener, PastePopupMenu {
    private final View mParent;
    private final PastePopupMenuDelegate mDelegate;
    private final Context mContext;
    private final PopupWindow mContainer;
    private int mRawPositionX;
    private int mRawPositionY;
    private int mPositionX;
    private int mPositionY;
    private int mStatusBarHeight;
    private View mPasteView;
    private final int mPasteViewLayout;
    private final int mLineOffsetY;
    private final int mWidthOffsetX;

    public LegacyPastePopupMenu(View parent, PastePopupMenuDelegate delegate) {
        mParent = parent;
        mDelegate = delegate;
        mContext = parent.getContext();
        mContainer = new PopupWindow(mContext, null, android.R.attr.textSelectHandleWindowStyle);
        mContainer.setSplitTouchEnabled(true);
        mContainer.setClippingEnabled(false);
        mContainer.setAnimationStyle(0);

        mContainer.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        mContainer.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);

        final int[] popupLayoutAttrs = {
                android.R.attr.textEditPasteWindowLayout,
        };

        mPasteView = null;
        TypedArray attrs = mContext.getTheme().obtainStyledAttributes(popupLayoutAttrs);
        mPasteViewLayout = attrs.getResourceId(attrs.getIndex(0), 0);

        attrs.recycle();

        // Convert line offset dips to pixels.
        mLineOffsetY = (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, 5.0f, mContext.getResources().getDisplayMetrics());
        mWidthOffsetX = (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, 30.0f, mContext.getResources().getDisplayMetrics());

        final int statusBarHeightResourceId =
                mContext.getResources().getIdentifier("status_bar_height", "dimen", "android");
        if (statusBarHeightResourceId > 0) {
            mStatusBarHeight =
                    mContext.getResources().getDimensionPixelSize(statusBarHeightResourceId);
        }
    }

    @Override
    public void show(int x, int y) {
        updateContent();
        positionAt(x, y);
    }

    @Override
    public void hide() {
        mContainer.dismiss();
    }

    @Override
    public boolean isShowing() {
        return mContainer.isShowing();
    }

    @Override
    public void onClick(View v) {
        paste();
        hide();
    }

    private void positionAt(int x, int y) {
        if (mRawPositionX == x && mRawPositionY == y && isShowing()) return;
        mRawPositionX = x;
        mRawPositionY = y;

        final View contentView = mContainer.getContentView();
        final int width = contentView.getMeasuredWidth();
        final int height = contentView.getMeasuredHeight();

        mPositionX = (int) (x - width / 2.0f);
        mPositionY = y - height - mLineOffsetY;

        final int[] coords = new int[2];
        mParent.getLocationInWindow(coords);
        coords[0] += mPositionX;
        coords[1] += mPositionY;

        int minOffsetY = 0;
        if (mParent.getSystemUiVisibility() == View.SYSTEM_UI_FLAG_VISIBLE) {
            minOffsetY = mStatusBarHeight;
        }

        final int screenWidth = mContext.getResources().getDisplayMetrics().widthPixels;
        if (coords[1] < minOffsetY) {
            // Vertical clipping, move under edited line and to the side of insertion cursor
            // TODO bottom clipping in case there is no system bar
            coords[1] += height;
            coords[1] += mLineOffsetY;

            // Move to right hand side of insertion cursor by default. TODO RTL text.
            final int handleHalfWidth = mWidthOffsetX / 2;

            if (x + width < screenWidth) {
                coords[0] += handleHalfWidth + width / 2;
            } else {
                coords[0] -= handleHalfWidth + width / 2;
            }
        } else {
            // Horizontal clipping
            coords[0] = Math.max(0, coords[0]);
            coords[0] = Math.min(screenWidth - width, coords[0]);
        }

        if (!isShowing()) {
            mContainer.showAtLocation(mParent, Gravity.NO_GRAVITY, coords[0], coords[1]);
        } else {
            mContainer.update(coords[0], coords[1], -1, -1);
        }
    }

    private void updateContent() {
        if (mPasteView == null) {
            final int layout = mPasteViewLayout;
            LayoutInflater inflater =
                    (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            if (inflater != null) {
                mPasteView = inflater.inflate(layout, null);
            }

            if (mPasteView == null) {
                throw new IllegalArgumentException("Unable to inflate TextEdit paste window");
            }

            final int size = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
            mPasteView.setLayoutParams(new LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            mPasteView.measure(size, size);

            mPasteView.setOnClickListener(this);
        }

        mContainer.setContentView(mPasteView);
    }

    private void paste() {
        mDelegate.paste();
    }
}
