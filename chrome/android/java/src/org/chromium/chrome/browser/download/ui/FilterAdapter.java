// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.os.AsyncTask;
import android.os.Environment;
import android.support.annotation.LayoutRes;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.download.DirectoryOption;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi.DownloadUiObserver;
import org.chromium.chrome.browser.util.FeatureUtilities;

import java.io.File;

/** An adapter that allows selecting an item from a dropdown spinner. */
class FilterAdapter
        extends BaseAdapter implements AdapterView.OnItemSelectedListener, DownloadUiObserver {
    private DownloadManagerUi mManagerUi;
    private DirectoryOption mDirectoryOption;
    private DefaultDirectoryTask mDirectorySpaceTask;

    /**
     * Asynchronous task to query the default download directory option on primary storage.
     * Pass one String parameter as the name of the directory option.
     */
    private static class DefaultDirectoryTask extends AsyncTask<String, Void, DirectoryOption> {
        @Override
        protected DirectoryOption doInBackground(String... params) {
            assert params.length == 1;
            File defaultDownloadDir =
                    Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
            DirectoryOption directoryOption = new DirectoryOption(params[0],
                    defaultDownloadDir.getAbsolutePath(), defaultDownloadDir.getUsableSpace(),
                    defaultDownloadDir.getTotalSpace(), DirectoryOption.DEFAULT_OPTION);
            return directoryOption;
        }
    }

    @Override
    public int getCount() {
        return DownloadFilter.getFilterCount();
    }

    @Override
    public Object getItem(int position) {
        return DownloadFilter.FILTER_LIST[position];
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getDropDownView(int position, View convertView, ViewGroup parent) {
        TextView labelView = (TextView) getViewFromResource(
                convertView, R.layout.download_manager_spinner_drop_down);
        labelView.setText(DownloadFilter.getStringIdForFilter(position));
        int iconId = DownloadFilter.getDrawableForFilter(position);
        VectorDrawableCompat iconDrawable =
                VectorDrawableCompat.create(mManagerUi.getActivity().getResources(), iconId,
                        mManagerUi.getActivity().getTheme());
        iconDrawable.setTintList(ApiCompatibilityUtils.getColorStateList(
                mManagerUi.getActivity().getResources(), R.color.dark_mode_tint));
        labelView.setCompoundDrawablesWithIntrinsicBounds(iconDrawable, null, null, null);

        return labelView;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View labelView = getViewFromResource(convertView, R.layout.download_manager_spinner);

        CharSequence title = mManagerUi.getActivity().getResources().getText(position == 0
                        ? R.string.menu_downloads
                        : DownloadFilter.getStringIdForFilter(position));

        TextView titleView = labelView.findViewById(R.id.download_home_title);
        titleView.setText(title);

        // Set the storage summary for download location change feature.
        TextView summaryView = labelView.findViewById(R.id.download_storage_summary);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE)
                && mDirectoryOption != null) {
            String storageSummary =
                    mManagerUi.getActivity().getString(R.string.download_manager_ui_space_using,
                            DownloadUtils.getStringForBytes(
                                    mManagerUi.getActivity(), mDirectoryOption.availableSpace),
                            DownloadUtils.getStringForBytes(
                                    mManagerUi.getActivity(), mDirectoryOption.totalSpace));
            summaryView.setText(storageSummary);
        } else {
            summaryView.setVisibility(View.GONE);
        }

        if (!FeatureUtilities.isChromeModernDesignEnabled()) {
            ApiCompatibilityUtils.setTextAppearance(titleView, R.style.BlackHeadline2);
        }
        return labelView;
    }

    private View getViewFromResource(View convertView, @LayoutRes int resId) {
        if (convertView != null) return convertView;
        return LayoutInflater.from(mManagerUi.getActivity()).inflate(resId, null);
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        mManagerUi.onFilterChanged(position);
    }

    public void initialize(DownloadManagerUi manager) {
        mManagerUi = manager;

        // Show storage summary.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE)) {
            mDirectorySpaceTask = new DefaultDirectoryTask() {
                @Override
                protected void onPostExecute(DirectoryOption directoryOption) {
                    mDirectoryOption = directoryOption;
                    notifyDataSetChanged();
                }
            };
            mDirectorySpaceTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, "");
        }
    }

    @Override
    public void onFilterChanged(int filter) {}

    @Override
    public void onManagerDestroyed() {
        mManagerUi = null;
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {}
}
