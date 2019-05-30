// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.v7.content.res.AppCompatResources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.widget.ChromeImageView;

import java.util.Calendar;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Adapter to populate the Target Device Picker sheet.
 */
public class DevicePickerBottomSheetAdapter extends BaseAdapter {
    private final LayoutInflater mInflator;
    private final Context mContext;
    private final List<TargetDeviceInfo> mTargetDevices;

    public DevicePickerBottomSheetAdapter(Profile profile) {
        mContext = ContextUtils.getApplicationContext();
        mInflator = LayoutInflater.from(mContext);
        mTargetDevices = SendTabToSelfAndroidBridge.getAllTargetDeviceInfos(profile);
    }

    @Override
    public int getCount() {
        return mTargetDevices.size();
    }

    @Override
    public TargetDeviceInfo getItem(int position) {
        return mTargetDevices.get(position);
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

            TargetDeviceInfo deviceInfo = getItem(position);
            ChromeImageView deviceIcon = convertView.findViewById(R.id.device_icon);
            deviceIcon.setImageDrawable(getDrawableForDeviceType(deviceInfo));
            deviceIcon.setVisibility(View.VISIBLE);

            TextView deviceName = convertView.findViewById(R.id.device_name);
            deviceName.setText(deviceInfo.deviceName);

            TextView lastActive = convertView.findViewById(R.id.last_active);

            long numDaysDeviceActive = TimeUnit.MILLISECONDS.toDays(
                    Calendar.getInstance().getTimeInMillis() - deviceInfo.lastUpdatedTimestamp);
            lastActive.setText(getLastActiveMessage(numDaysDeviceActive));
        }
        return convertView;
    }

    private String getLastActiveMessage(long numDays) {
        if (numDays < 1) {
            return mContext.getResources().getString(
                    R.string.send_tab_to_self_device_last_active_today);
        } else if (numDays == 1) {
            return mContext.getResources().getString(
                    R.string.send_tab_to_self_device_last_active_one_day_ago);
        } else {
            return mContext.getResources().getString(
                    R.string.send_tab_to_self_device_last_active_more_than_one_day, numDays);
        }
    }

    private Drawable getDrawableForDeviceType(TargetDeviceInfo targetDevice) {
        switch (targetDevice.deviceType) {
            case TargetDeviceInfo.DeviceType.CHROMEOS:
            case TargetDeviceInfo.DeviceType.LINUX:
            case TargetDeviceInfo.DeviceType.MACOSX:
            case TargetDeviceInfo.DeviceType.WIN: {
                return AppCompatResources.getDrawable(mContext, R.drawable.computer_black_24dp);
            }
            case TargetDeviceInfo.DeviceType.PHONE: {
                return AppCompatResources.getDrawable(mContext, R.drawable.smartphone_black_24dp);
            }
        }
        return AppCompatResources.getDrawable(mContext, R.drawable.devices_black_24dp);
    }
}
