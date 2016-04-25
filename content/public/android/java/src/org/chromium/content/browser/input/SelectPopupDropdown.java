// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.widget.AdapterView;
import android.widget.PopupWindow;

import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.RenderCoordinates;
import org.chromium.ui.DropdownAdapter;
import org.chromium.ui.DropdownPopupWindow;

import java.util.List;

/**
 * Handles the dropdown popup for the <select> HTML tag support.
 */
public class SelectPopupDropdown implements SelectPopup {

    private final ContentViewCore mContentViewCore;
    private final Context mContext;
    private final DropdownPopupWindow mDropdownPopupWindow;

    private int mInitialSelection = -1;
    private boolean mSelectionNotified;

    public SelectPopupDropdown(ContentViewCore contentViewCore, List<SelectPopupItem> items,
            Rect bounds, int[] selected, boolean rightAligned) {
        mContentViewCore = contentViewCore;
        mContext = mContentViewCore.getContext();
        mDropdownPopupWindow = new DropdownPopupWindow(mContext,
                mContentViewCore.getViewAndroidDelegate());
        mDropdownPopupWindow.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                notifySelection(new int[] {position});
                hide(false);
            }
        });
        if (selected.length > 0) {
            mInitialSelection = selected[0];
        }
        mDropdownPopupWindow.setAdapter(new DropdownAdapter(mContext, items, null));
        mDropdownPopupWindow.setRtl(rightAligned);
        RenderCoordinates renderCoordinates = mContentViewCore.getRenderCoordinates();
        float anchorX = renderCoordinates.fromPixToDip(
                renderCoordinates.fromLocalCssToPix(bounds.left));
        float anchorY = renderCoordinates.fromPixToDip(
                renderCoordinates.fromLocalCssToPix(bounds.top));
        float anchorWidth = renderCoordinates.fromPixToDip(
                renderCoordinates.fromLocalCssToPix(bounds.right)) - anchorX;
        float anchorHeight = renderCoordinates.fromPixToDip(
                renderCoordinates.fromLocalCssToPix(bounds.bottom)) - anchorY;
        mDropdownPopupWindow.setAnchorRect(anchorX, anchorY, anchorWidth, anchorHeight);
        mDropdownPopupWindow.setOnDismissListener(
                new PopupWindow.OnDismissListener() {
                    @Override
                    public void onDismiss() {
                        notifySelection(null);
                    }
                });
    }

    private void notifySelection(int[] indicies) {
        if (mSelectionNotified) return;
        mContentViewCore.selectPopupMenuItems(indicies);
        mSelectionNotified = true;
    }

    @Override
    public void show() {
        mDropdownPopupWindow.show();
        if (mInitialSelection >= 0) {
            mDropdownPopupWindow.getListView().setSelection(mInitialSelection);
        }
    }

    @Override
    public void hide(boolean sendsCancelMessage) {
        if (sendsCancelMessage) {
            mDropdownPopupWindow.dismiss();
            notifySelection(null);
        } else {
            mSelectionNotified = true;
            mDropdownPopupWindow.dismiss();
        }
    }
}
