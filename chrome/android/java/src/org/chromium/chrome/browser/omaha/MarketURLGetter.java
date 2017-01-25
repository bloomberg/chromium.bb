// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Looper;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;

/**
 * Grabs the URL that points to the Android Market page for Chrome.
 * This incurs I/O, so don't use it from the main thread.
 */
public class MarketURLGetter {

    private static final class LazyHolder {
        private static final MarketURLGetter INSTANCE = new MarketURLGetter();
    }

    /** See {@link #getMarketUrl(Context, String, String)} */
    static String getMarketUrl(Context context) {
        assert !ThreadUtils.runningOnUiThread();
        MarketURLGetter instance =
                sInstanceForTests == null ? LazyHolder.INSTANCE : sInstanceForTests;
        return instance.getMarketUrl(
                context, OmahaClient.PREF_PACKAGE, OmahaClient.PREF_MARKET_URL);
    }

    @VisibleForTesting
    static void setInstanceForTests(MarketURLGetter getter) {
        sInstanceForTests = getter;
    }

    private static MarketURLGetter sInstanceForTests;

    protected MarketURLGetter() { }

    /** Returns the Play Store URL that points to Chrome. */
    public String getMarketUrl(
            Context applicationContext, String prefPackage, String prefMarketUrl) {
        assert Looper.myLooper() != Looper.getMainLooper();

        SharedPreferences prefs = applicationContext.getSharedPreferences(
                prefPackage, Context.MODE_PRIVATE);
        return prefs.getString(prefMarketUrl, "");
    }
}
