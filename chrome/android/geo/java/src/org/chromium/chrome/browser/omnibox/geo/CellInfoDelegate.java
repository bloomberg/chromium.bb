// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import android.telephony.CellInfo;
import android.telephony.TelephonyManager;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.task.AsyncTask;

import java.util.List;

/**
 * Wrapper class to delegate requests for {@link CellInfo} data to {@link TelephonyManager}.
 *
 * TODO(crbug.com/954620): Replace this class once P builds are no longer supported.
 */
class CellInfoDelegate {
    static void requestCellInfoUpdate(
            TelephonyManager telephonyManager, Callback<List<CellInfo>> callback) {
        // Apps targeting Q+ must call a different API to trigger a refresh of the cached CellInfo.
        // https://developer.android.com/reference/android/telephony/TelephonyManager.html#getAllCellInfo()
        if (BuildInfo.isAtLeastQ()) {
            requestCellInfoUpdateAtLeastQ(telephonyManager, callback);
            return;
        }

        requestCellInfoUpdatePreQ(telephonyManager, callback);
    }

    private static void requestCellInfoUpdateAtLeastQ(
            TelephonyManager telephonyManager, Callback<List<CellInfo>> callback) {
        telephonyManager.requestCellInfoUpdate(
                AsyncTask.THREAD_POOL_EXECUTOR, new TelephonyManager.CellInfoCallback() {
                    @Override
                    public void onCellInfo(List<CellInfo> cellInfos) {
                        callback.onResult(cellInfos);
                    }
                });
    }

    private static void requestCellInfoUpdatePreQ(
            TelephonyManager telephonyManager, Callback<List<CellInfo>> callback) {
        callback.onResult(telephonyManager.getAllCellInfo());
    }
}
