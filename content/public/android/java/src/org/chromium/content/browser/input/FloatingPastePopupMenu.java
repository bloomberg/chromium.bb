// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Build;
import android.util.TypedValue;
import android.view.ActionMode;
import android.view.View;

import org.chromium.content.browser.FloatingWebActionModeCallback;
import org.chromium.content.browser.WebActionModeCallback;

/**
 * Paste popup implementation based on floating ActionModes.
 */
@TargetApi(Build.VERSION_CODES.M)
public class FloatingPastePopupMenu implements PastePopupMenu {
    private static final int CONTENT_RECT_OFFSET_DIP = 15;
    private static final int SLOP_LENGTH_DIP = 10;

    private final View mParent;
    private final PastePopupMenuDelegate mDelegate;
    private final Context mContext;

    // Offset from the paste coordinates to provide the floating ActionMode.
    private final int mContentRectOffset;

    // Slack for ignoring small deltas in the paste popup position. The initial
    // position can change by a few pixels due to differences in how context
    // menu and selection coordinates are computed. Suppressing this small delta
    // avoids the floating ActionMode flicker when the popup is repositioned.
    private final int mSlopLengthSquared;

    private ActionMode mActionMode;
    private WebActionModeCallback.ActionHandler mActionHandler;
    private int mRawPositionX;
    private int mRawPositionY;

    // Embedders may not support floating ActionModes, in which case we should
    // use the legacy menu as a fallback.
    private LegacyPastePopupMenu mFallbackPastePopupMenu;

    public FloatingPastePopupMenu(View parent, PastePopupMenuDelegate delegate) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;

        mParent = parent;
        mDelegate = delegate;
        mContext = parent.getContext();

        mContentRectOffset = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                CONTENT_RECT_OFFSET_DIP, mContext.getResources().getDisplayMetrics());
        int slopLength = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                SLOP_LENGTH_DIP, mContext.getResources().getDisplayMetrics());
        mSlopLengthSquared = slopLength * slopLength;
    }

    @Override
    public void show(int x, int y) {
        if (mFallbackPastePopupMenu != null) {
            mFallbackPastePopupMenu.show(x, y);
            return;
        }

        if (isShowing()) {
            int dx = mRawPositionX - x;
            int dy = mRawPositionY - y;
            if (dx * dx + dy * dy < mSlopLengthSquared) return;
        }

        mRawPositionX = x;
        mRawPositionY = y;
        if (mActionMode != null) {
            mActionMode.invalidateContentRect();
            return;
        }

        ensureActionModeOrFallback();
    }

    @Override
    public void hide() {
        if (mFallbackPastePopupMenu != null) {
            mFallbackPastePopupMenu.hide();
            return;
        }

        if (mActionMode != null) {
            mActionMode.finish();
            mActionMode = null;
        }
    }

    @Override
    public boolean isShowing() {
        if (mFallbackPastePopupMenu != null) return mFallbackPastePopupMenu.isShowing();
        return mActionMode != null;
    }

    private void ensureActionModeOrFallback() {
        if (mActionMode != null) return;
        if (mFallbackPastePopupMenu != null) return;

        ActionMode.Callback2 callback2 = new FloatingWebActionModeCallback(
                new WebActionModeCallback(mParent.getContext(), getActionHandler()));
        ActionMode actionMode = mParent.startActionMode(callback2, ActionMode.TYPE_FLOATING);
        if (actionMode != null) {
            assert actionMode.getType() == ActionMode.TYPE_FLOATING;
            mActionMode = actionMode;
        } else {
            mFallbackPastePopupMenu = new LegacyPastePopupMenu(mParent, mDelegate);
            mFallbackPastePopupMenu.show(mRawPositionX, mRawPositionY);
        }
    }

    private WebActionModeCallback.ActionHandler getActionHandler() {
        if (mActionHandler != null) return mActionHandler;
        mActionHandler = new WebActionModeCallback.ActionHandler() {
            @Override
            public void selectAll() {}

            @Override
            public void cut() {}

            @Override
            public void copy() {}

            @Override
            public void paste() {
                mDelegate.paste();
            }

            @Override
            public void share() {}

            @Override
            public void search() {}

            @Override
            public void processText(Intent intent) {}

            @Override
            public boolean isSelectionPassword() {
                return false;
            }

            @Override
            public boolean isSelectionEditable() {
                return true;
            }

            @Override
            public boolean isInsertion() {
                return true;
            }

            @Override
            public void onDestroyActionMode() {
                mActionMode = null;
            }

            @Override
            public void onGetContentRect(Rect outRect) {
                // Use a rect that spans above and below the insertion point.
                // This avoids paste popup overlap with selection handles.
                outRect.set(mRawPositionX - mContentRectOffset, mRawPositionY - mContentRectOffset,
                        mRawPositionX + mContentRectOffset, mRawPositionY + mContentRectOffset);
            }

            @Override
            public boolean isIncognito() {
                return false;
            }

            @Override
            public boolean isSelectActionModeAllowed(int actionModeItem) {
                return false;
            }
        };
        return mActionHandler;
    }
}
