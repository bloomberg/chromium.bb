// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;

/**
 * Bottom sheet content to display a list of devices a user can send a tab to after they have
 * chosen to share it with themselves through the SendTabToSelfFeature.
 */
public class DevicePickerBottomSheetContent implements BottomSheetContent, OnItemClickListener {
    Context mContext;
    ChromeActivity mActivity;
    ViewGroup mToolbarView;
    ViewGroup mContentView;
    DevicePickerBottomSheetAdapter mAdapter;

    public DevicePickerBottomSheetContent(Context context, ChromeActivity activity) {
        mContext = context;
        mActivity = activity;
        mAdapter = new DevicePickerBottomSheetAdapter();

        createToolbarView();
    }

    private void createToolbarView() {
        mToolbarView = (ViewGroup) LayoutInflater.from(mContext).inflate(
                R.layout.send_tab_to_self_device_picker_toolbar, null);
        TextView toolbarText = mToolbarView.findViewById(R.id.device_picker_toolbar);
        toolbarText.setText(R.string.send_tab_to_self_sheet_toolbar);
    }

    private void createContentView() {
        mContentView = (ViewGroup) LayoutInflater.from(mContext).inflate(
                R.layout.send_tab_to_self_device_picker_list, null);
        ListView listView = mContentView.findViewById(R.id.device_picker_list);

        // Set the padding so that the toolbar is aligned with the list.
        // TODO(tgupta): Figure out whether this can be incorporated directly in the definition of
        // the list view rather than set programatically.
        // listView.setPadding(0, convertDpToPx(80), 0, 0);
        listView.setAdapter(mAdapter);
        listView.setOnItemClickListener(this);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    private int convertDpToPx(int inDp) {
        float scale = mContext.getResources().getDisplayMetrics().density;
        return (int) (inDp * scale + 0.5f);
    }

    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheet.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        // This ensures that the bottom sheet reappears after the first time. Otherwise, the
        // second time that a user initiates a share, the bottom sheet does not re-appear.
        return true;
    }

    @Override
    public boolean isPeekStateEnabled() {
        // Return false to ensure that the entire bottom sheet is shown.
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.send_tab_to_self_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_closed;
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        // TODO(crbug.com/949223): Add logic to support tapping on a device.
    }
}
