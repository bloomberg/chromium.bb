// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.ui.widget.ChromeImageView;

/**
 * Adapter to populate the Target Device Picker sheet.
 */
public class DevicePickerBottomSheetAdapter extends BaseAdapter {
    private final LayoutInflater mInflator;
    private final Context mContext;

    public DevicePickerBottomSheetAdapter() {
        mContext = ContextUtils.getApplicationContext();
        mInflator = LayoutInflater.from(mContext);
    }

    @Override
    public int getCount() {
        return 0;
    }

    @Override
    public Object getItem(int position) {
        return null;
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        if (convertView == null) {
            convertView =
                    mInflator.inflate(R.layout.send_tab_to_self_device_picker_item, parent, false);

            ChromeImageView deviceIcon = convertView.findViewById(R.id.device_icon);
            /// TODO(crbug.com/949223): Populate with the right icon here.
            // deviceIcon.setImageDrawable(
            // AppCompatResources.getDrawable(mContext, R.drawable.ic_check_googblue_24dp));
            deviceIcon.setVisibility(View.VISIBLE);

            // TODO(crbug.com/949223): Populate the device name here.
            // TextView deviceName = convertView.findViewById(R.id.device_name);
            // deviceName.setText(getItem(position));
            // TODO(crbug.com/949223): Add logic to handle click action
        }
        return convertView;
    }
}
