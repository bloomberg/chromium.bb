// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.download;

import android.content.Context;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.widget.TextViewCompat;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.widget.TintedImageView;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/**
 * Custom adapter that populates the list of which directories the user can choose as their default
 * download location.
 */
public class DownloadDirectoryAdapter extends ArrayAdapter<Object> {
    public static int NO_SELECTED_ITEM_ID = -1;

    /**
     * Denotes a given option for directory selection; includes name, location, and space.
     */
    public static class DirectoryOption {
        private final String mName;
        private final File mLocation;
        private final long mAvailableSpace;

        DirectoryOption(String directoryName, File directoryLocation, long availableSpace) {
            mName = directoryName;
            mLocation = directoryLocation;
            mAvailableSpace = availableSpace;
        }

        /**
         * @return File location associated with this directory option.
         */
        public File getLocation() {
            return mLocation;
        }

        /**
         * @return Name associated with this directory option.
         */
        public String getName() {
            return mName;
        }

        /**
         * @return Amount of available space in this directory option.
         */
        long getAvailableSpace() {
            return mAvailableSpace;
        }
    }

    private Context mContext;
    private LayoutInflater mLayoutInflater;

    private List<DirectoryOption> mCanonicalOptions = new ArrayList<>();
    private List<DirectoryOption> mAdditionalOptions = new ArrayList<>();
    private List<DirectoryOption> mErrorOptions = new ArrayList<>();

    public DownloadDirectoryAdapter(@NonNull Context context) {
        super(context, android.R.layout.simple_spinner_item);

        mContext = context;
        mLayoutInflater = LayoutInflater.from(context);

        refreshData();
    }

    @Override
    public int getCount() {
        return mCanonicalOptions.size() + mAdditionalOptions.size() + mErrorOptions.size();
    }

    @Nullable
    @Override
    public Object getItem(int position) {
        if (!mErrorOptions.isEmpty()) {
            assert position == 0;
            assert getCount() == 1;
            return mErrorOptions.get(position);
        }

        if (position < mCanonicalOptions.size()) {
            return mCanonicalOptions.get(position);
        } else {
            return mAdditionalOptions.get(position - mCanonicalOptions.size());
        }
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @NonNull
    @Override
    public View getView(int position, @Nullable View convertView, @NonNull ViewGroup parent) {
        View view = convertView;
        if (view == null) {
            view = mLayoutInflater.inflate(R.layout.download_location_spinner_item, null);
        }

        view.setTag(position);

        DirectoryOption directoryOption = (DirectoryOption) getItem(position);
        if (directoryOption == null) return view;

        TextView titleText = (TextView) view.findViewById(R.id.text);
        titleText.setText(directoryOption.getName());

        return view;
    }

    @Override
    public View getDropDownView(
            int position, @Nullable View convertView, @NonNull ViewGroup parent) {
        View view = convertView;
        if (view == null) {
            view = mLayoutInflater.inflate(R.layout.download_location_spinner_dropdown_item, null);
        }

        view.setTag(position);

        DirectoryOption directoryOption = (DirectoryOption) getItem(position);
        if (directoryOption == null) return view;

        TextView titleText = (TextView) view.findViewById(R.id.title);
        titleText.setText(directoryOption.getName());

        TextView summaryText = (TextView) view.findViewById(R.id.description);
        if (isEnabled(position)) {
            TextViewCompat.setTextAppearance(titleText, R.style.BlackTitle1);
            TextViewCompat.setTextAppearance(summaryText, R.style.BlackBody);
            summaryText.setText(DownloadUtils.getStringForAvailableBytes(
                    mContext, directoryOption.getAvailableSpace()));
        } else {
            TextViewCompat.setTextAppearance(titleText, R.style.BlackDisabledText1);
            TextViewCompat.setTextAppearance(summaryText, R.style.BlackDisabledText3);
            if (mErrorOptions.isEmpty()) {
                summaryText.setText(mContext.getText(R.string.download_location_not_enough_space));
            } else {
                summaryText.setVisibility(View.GONE);
            }
        }

        TintedImageView imageView = (TintedImageView) view.findViewById(R.id.icon_view);
        imageView.setVisibility(View.GONE);

        return view;
    }

    @Override
    public boolean isEnabled(int position) {
        DirectoryOption directoryOption = (DirectoryOption) getItem(position);
        return directoryOption != null && directoryOption.getAvailableSpace() != 0;
    }

    /**
     * @return  ID of the directory option that matches the default download location or
     *          NO_SELECTED_ITEM_ID if no item matches the default path.
     */
    public int getSelectedItemId() {
        if (!mErrorOptions.isEmpty()) return 0;
        String defaultLocation = PrefServiceBridge.getInstance().getDownloadDefaultDirectory();
        for (int i = 0; i < getCount(); i++) {
            DirectoryOption option = (DirectoryOption) getItem(i);
            if (option == null) continue;
            if (defaultLocation.equals(option.getLocation().getAbsolutePath())) return i;
        }
        return NO_SELECTED_ITEM_ID;
    }

    /**
     * In the case that there is no selected item ID/the selected item ID is invalid (ie. there is
     * not enough space), select either the default or the next valid item ID. Set the default to be
     * this item and return the ID.
     *
     * @return  ID of the first valid, selectable item and the new default location.
     */
    public int getFirstSelectableItemId() {
        for (int i = 0; i < getCount(); i++) {
            DirectoryOption option = (DirectoryOption) getItem(i);
            if (option == null) continue;
            if (option.getAvailableSpace() > 0) {
                PrefServiceBridge.getInstance().setDownloadAndSaveFileDefaultDirectory(
                        option.getLocation().getAbsolutePath());
                return i;
            }
        }

        // Display an option that says there are no available download locations.
        adjustErrorDirectoryOption();
        return 0;
    }

    boolean hasAvailableLocations() {
        return mErrorOptions.isEmpty();
    }

    private void refreshData() {
        setCanonicalDirectoryOptions();
        setAdditionalDirectoryOptions();
        adjustErrorDirectoryOption();
    }

    private void setCanonicalDirectoryOptions() {
        mCanonicalOptions.clear();
        File directoryLocation =
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        mCanonicalOptions.add(new DirectoryOption(mContext.getString(R.string.menu_downloads),
                directoryLocation, directoryLocation.getUsableSpace()));
    }

    private void setAdditionalDirectoryOptions() {
        mAdditionalOptions.clear();

        // If there are no more additional directories, it is only the primary storage available.
        File[] externalDirs = DownloadUtils.getAllDownloadDirectories(mContext);
        if (externalDirs.length <= 1) return;

        int numOtherAdditionalDirectories = 0;
        for (File dir : externalDirs) {
            if (dir == null) continue;

            // Skip the directory that is in primary storage.
            if (dir.getAbsolutePath().contains(
                        Environment.getExternalStorageDirectory().getAbsolutePath())) {
                continue;
            }

            // Add index (ie. SD Card 2) if there is more than one secondary storage option.
            String directoryName = (numOtherAdditionalDirectories > 0)
                    ? mContext.getString(
                              org.chromium.chrome.R.string.downloads_location_sd_card_number,
                              numOtherAdditionalDirectories + 1)
                    : mContext.getString(org.chromium.chrome.R.string.downloads_location_sd_card);

            mAdditionalOptions.add(new DirectoryOption(directoryName, dir, dir.getUsableSpace()));
            numOtherAdditionalDirectories++;
        }
    }

    private void adjustErrorDirectoryOption() {
        if ((mCanonicalOptions.size() + mAdditionalOptions.size()) > 0) {
            mErrorOptions.clear();
        } else {
            mErrorOptions.add(new DirectoryOption(
                    mContext.getString(R.string.download_location_no_available_locations), null,
                    0));
        }
    }
}
