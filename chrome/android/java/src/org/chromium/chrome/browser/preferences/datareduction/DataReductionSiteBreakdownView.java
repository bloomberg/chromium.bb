// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.datareduction;

import android.content.Context;
import android.text.format.Formatter;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.TableLayout;
import android.widget.TableRow;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;

import java.io.Serializable;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * A site breakdown view to be used by the Data Saver settings page. It displays the top ten sites
 * with the most data use or data savings.
 */
public class DataReductionSiteBreakdownView extends LinearLayout {
    private static final int NUM_DATA_USE_ITEMS_TO_DISPLAY = 10;

    private TableLayout mTableLayout;

    public DataReductionSiteBreakdownView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTableLayout = (TableLayout) findViewById(R.id.data_reduction_proxy_breakdown_table);
    }

    /**
     * Display the data use items once they have been fetched from the compression stats.
     * @param items A list of items split by hostname to show in the breakdown.
     */
    public void onQueryDataUsageComplete(List<DataReductionDataUseItem> items) {
        updateSiteBreakdown(items);
    }

    /**
     * Sorts the DataReductionDataUseItems by most to least data used.
     */
    private static final class DataUsedComparator
            implements Comparator<DataReductionDataUseItem>, Serializable {
        @Override
        public int compare(DataReductionDataUseItem lhs, DataReductionDataUseItem rhs) {
            if (lhs.getDataUsed() < rhs.getDataUsed()) {
                return 1;
            } else if (lhs.getDataUsed() > rhs.getDataUsed()) {
                return -1;
            }
            return 0;
        }
    }

    /**
     * Update the site breakdown to display the given date use items.
     * @param items A list of items split by hostname to show in the breakdown.
     */
    private void updateSiteBreakdown(List<DataReductionDataUseItem> items) {
        if (items.size() == 0) {
            setVisibility(GONE);
            return;
        }

        setVisibility(VISIBLE);
        // Remove all old rows except the header.
        mTableLayout.removeViews(1, mTableLayout.getChildCount() - 1);
        final DataUsedComparator comp = new DataUsedComparator();
        Collections.sort(items, comp);

        int numRemainingSites = 0;
        int everythingElseDataUsage = 0;
        int everythingElseDataSavings = 0;

        for (int i = 0; i < items.size(); i++) {
            if (i < NUM_DATA_USE_ITEMS_TO_DISPLAY) {
                TableRow row = (TableRow) LayoutInflater.from(getContext())
                                       .inflate(R.layout.data_usage_breakdown_row, null);

                TextView hostnameView = (TextView) row.findViewById(R.id.site_hostname);
                TextView dataUsedView = (TextView) row.findViewById(R.id.site_data_used);
                TextView dataSavedView = (TextView) row.findViewById(R.id.site_data_saved);

                hostnameView.setText(items.get(i).getHostname());
                dataUsedView.setText(items.get(i).getFormattedDataUsed(getContext()));
                dataSavedView.setText(items.get(i).getFormattedDataSaved(getContext()));

                mTableLayout.addView(row, i + 1);
            } else {
                numRemainingSites++;
                everythingElseDataUsage += items.get(i).getDataUsed();
                everythingElseDataSavings += items.get(i).getDataSaved();
            }
        }

        if (numRemainingSites > 0) {
            TableRow row = (TableRow) LayoutInflater.from(getContext())
                                   .inflate(R.layout.data_usage_breakdown_row, null);

            TextView hostnameView = (TextView) row.findViewById(R.id.site_hostname);
            TextView dataUsedView = (TextView) row.findViewById(R.id.site_data_used);
            TextView dataSavedView = (TextView) row.findViewById(R.id.site_data_saved);

            hostnameView.setText(getResources().getString(
                    R.string.data_reduction_breakdown_remaining_sites_label, numRemainingSites));
            dataUsedView.setText(Formatter.formatFileSize(getContext(), everythingElseDataUsage));
            dataSavedView.setText(
                    Formatter.formatFileSize(getContext(), everythingElseDataSavings));

            int lightActiveColor = ApiCompatibilityUtils.getColor(
                    getContext().getResources(), R.color.light_active_color);

            hostnameView.setTextColor(lightActiveColor);
            dataUsedView.setTextColor(lightActiveColor);
            dataSavedView.setTextColor(lightActiveColor);

            mTableLayout.addView(row, NUM_DATA_USE_ITEMS_TO_DISPLAY + 1);
        }

        mTableLayout.requestLayout();
    }
}