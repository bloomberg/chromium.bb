// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.view.View;
import android.widget.AdapterView;
import android.widget.PopupWindow;

import org.chromium.content_public.browser.ContentViewCore;
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

    private boolean mSelectionNotified;

    public SelectPopupDropdown(ContentViewCore contentViewCore, View anchorView,
            List<SelectPopupItem> items, int[] selected, boolean rightAligned) {
        mContentViewCore = contentViewCore;
        mContext = mContentViewCore.getContext();
        mDropdownPopupWindow = new DropdownPopupWindow(mContext, anchorView);
        mDropdownPopupWindow.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                notifySelection(new int[] {position});
                hide(false);
            }
        });

        int initialSelection = -1;
        if (selected.length > 0) {
            initialSelection = selected[0];
        }
        mDropdownPopupWindow.setInitialSelection(initialSelection);
        mDropdownPopupWindow.setAdapter(new DropdownAdapter(
                mContext, items, null /* separators */, null /* backgroundColor */,
                null /* dividerColor */, null /* dropdownItemHeight */, null /* margin */));
        mDropdownPopupWindow.setRtl(rightAligned);
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
        // postShow() to make sure show() happens after the layout of the anchor view has been
        // changed.
        mDropdownPopupWindow.postShow();
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
