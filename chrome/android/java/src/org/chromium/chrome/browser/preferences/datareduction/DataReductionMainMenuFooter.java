// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.datareduction;

import android.content.Context;
import android.content.Intent;
import android.text.format.DateUtils;
import android.text.format.Formatter;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.third_party.android.datausagechart.ChartDataUsageView;

/**
 * Specific {@link LinearLayout} that is displays the data savings of Data Saver in the main menu
 * footer.
 */
public class DataReductionMainMenuFooter extends LinearLayout implements View.OnClickListener {
    /**
     * Constructs a new {@link DataReductionMainMenuFooter} with the appropriate context.
     */
    public DataReductionMainMenuFooter(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        TextView textSummary = (TextView) findViewById(R.id.menu_item_summary);

        if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) {
            String dataSaved = Formatter.formatShortFileSize(getContext(),
                    DataReductionProxySettings.getInstance().getTotalHttpContentLengthSaved());

            long millisSinceEpoch =
                    DataReductionProxySettings.getInstance().getDataReductionLastUpdateTime()
                    - DateUtils.DAY_IN_MILLIS * ChartDataUsageView.DAYS_IN_CHART;
            final int flags = DateUtils.FORMAT_NUMERIC_DATE | DateUtils.FORMAT_NO_YEAR;
            String date =
                    DateUtils.formatDateTime(getContext(), millisSinceEpoch, flags).toString();

            textSummary.setText(
                    getContext().getString(R.string.data_reduction_saved_label, dataSaved, date));
        } else {
            textSummary.setText(R.string.text_off);
        }

        setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                getContext(), DataReductionPreferences.class.getName());
        RecordUserAction.record("MobileMenuDataSaverOpened");
        intent.putExtra(DataReductionPreferences.FROM_MAIN_MENU, true);
        getContext().startActivity(intent);
    }
}