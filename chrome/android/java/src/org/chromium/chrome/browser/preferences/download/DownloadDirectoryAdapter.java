// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.download;

import android.content.Context;
import android.os.Build;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.format.Formatter;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import org.chromium.chrome.R;
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
    /**
     * Denotes a given option for directory selection; includes name, location, and space.
     */
    public static class DirectoryOption {
        private String mName;
        private File mLocation;
        private String mAvailableSpace;

        DirectoryOption(String directoryName, File directoryLocation, String availableSpace) {
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
        String getAvailableSpace() {
            return mAvailableSpace;
        }
    }

    private Context mContext;
    private LayoutInflater mLayoutInflater;

    private List<DirectoryOption> mCanonicalOptions = new ArrayList<>();
    private List<DirectoryOption> mAdditionalOptions = new ArrayList<>();

    public DownloadDirectoryAdapter(@NonNull Context context) {
        super(context, android.R.layout.simple_spinner_item);

        mContext = context;
        mLayoutInflater = LayoutInflater.from(context);

        refreshData();
    }

    @Override
    public int getCount() {
        return mCanonicalOptions.size() + mAdditionalOptions.size();
    }

    @Nullable
    @Override
    public Object getItem(int position) {
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
        summaryText.setText(directoryOption.getAvailableSpace());

        TintedImageView imageView = (TintedImageView) view.findViewById(R.id.icon_view);
        imageView.setVisibility(View.GONE);

        return view;
    }

    /**
     * @return Id of the directory option that matches the default download location.
     */
    public int getSelectedItemId() {
        String location = PrefServiceBridge.getInstance().getDownloadDefaultDirectory();
        for (int i = 0; i < getCount(); i++) {
            DirectoryOption option = (DirectoryOption) getItem(i);
            if (option == null) continue;
            if (location.equals(option.getLocation().getAbsolutePath())) return i;
        }
        throw new AssertionError("No selected item ID.");
    }

    private void refreshData() {
        setCanonicalDirectoryOptions();
        setAdditionalDirectoryOptions();
    }

    private void setCanonicalDirectoryOptions() {
        mCanonicalOptions.clear();
        File directoryLocation =
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        mCanonicalOptions.add(new DirectoryOption(mContext.getString(R.string.menu_downloads),
                directoryLocation, getAvailableBytesString(directoryLocation)));
    }

    private void setAdditionalDirectoryOptions() {
        mAdditionalOptions.clear();

        // TODO(jming): Is there any way to do this for API < 19????
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) return;

        File[] externalDirs = mContext.getExternalFilesDirs(Environment.DIRECTORY_DOWNLOADS);

        // If there are no more additional directories, it is only the primary storage available.
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
            String availableBytes = getAvailableBytesString(dir);

            mAdditionalOptions.add(new DirectoryOption(directoryName, dir, availableBytes));
            numOtherAdditionalDirectories++;
        }
    }

    private String getAvailableBytesString(File file) {
        return Formatter.formatFileSize(mContext, file.getUsableSpace());
    }
}
