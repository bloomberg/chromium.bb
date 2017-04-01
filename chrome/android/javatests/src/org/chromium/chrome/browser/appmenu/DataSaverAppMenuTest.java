// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.app.Activity;
import android.support.test.filters.SmallTest;
import android.test.UiThreadTest;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;

/**
 * Tests the Data Saver AppMenu footer
 */
@RetryOnFailure
public class DataSaverAppMenuTest extends ChromeActivityTestCaseBase<ChromeTabbedActivity> {
    private AppMenuHandlerForTest mAppMenuHandler;

    /**
     * AppMenuHandler that will be used to intercept the delegate for testing.
     */
    public static class AppMenuHandlerForTest extends AppMenuHandler {
        AppMenuPropertiesDelegate mDelegate;

        /**
         * AppMenuHandler for intercepting options item selections.
         */
        public AppMenuHandlerForTest(
                Activity activity, AppMenuPropertiesDelegate delegate, int menuResourceId) {
            super(activity, delegate, menuResourceId);
            mDelegate = delegate;
        }

        public AppMenuPropertiesDelegate getDelegate() {
            return mDelegate;
        }
    }

    public DataSaverAppMenuTest() {
        super(ChromeTabbedActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void setUp() throws Exception {
        ChromeTabbedActivity.setAppMenuHandlerFactoryForTesting(
                new ChromeTabbedActivity.AppMenuHandlerFactory() {
                    @Override
                    public AppMenuHandler get(Activity activity, AppMenuPropertiesDelegate delegate,
                            int menuResourceId) {
                        mAppMenuHandler =
                                new AppMenuHandlerForTest(activity, delegate, menuResourceId);
                        return mAppMenuHandler;
                    }
                });

        super.setUp();
    }

    /**
     * Verify the Data Saver item does not show when the feature isn't on, and the proxy is enabled.
     */
    @SmallTest
    @UiThreadTest
    @CommandLineFlags.Add("disable-field-trial-config")
    @Feature({"Browser", "Main"})
    public void testMenuDataSaverNoFeature() {
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        assertEquals(0, mAppMenuHandler.getDelegate().getFooterResourceId());
        DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                getActivity().getApplicationContext(), true);
        assertEquals(0, mAppMenuHandler.getDelegate().getFooterResourceId());
    }

    /**
     * Verify the Data Saver footer shows with the flag when the proxy is on.
     */
    @SmallTest
    @UiThreadTest
    @CommandLineFlags.Add({"enable-features=DataReductionProxyMainMenu",
            "disable-field-trial-config"})
    @Feature({"Browser", "Main"})
    public void testMenuDataSaver() {
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        // Data Saver hasn't been turned on, the footer shouldn't show.
        assertEquals(0, mAppMenuHandler.getDelegate().getFooterResourceId());

        // Turn Data Saver on, the footer should show.
        DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                getActivity().getApplicationContext(), true);
        assertEquals(R.layout.data_reduction_main_menu_footer,
                mAppMenuHandler.getDelegate().getFooterResourceId());

        // Ensure the footer is removed if the proxy is turned off.
        DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                getActivity().getApplicationContext(), false);
        assertEquals(0, mAppMenuHandler.getDelegate().getFooterResourceId());
    }

    /**
     * Verify the Data Saver footer shows with the flag when the proxy turns on and remains in the
     * main menu.
     */
    @SmallTest
    @UiThreadTest
    @CommandLineFlags.Add({"enable-features=DataReductionProxyMainMenu<DataReductionProxyMainMenu",
            "force-fieldtrials=DataReductionProxyMainMenu/Enabled",
            "force-fieldtrial-params=DataReductionProxyMainMenu.Enabled:"
                    + "persistent_menu_item_enabled/true",
            "disable-field-trial-config"})
    @Feature({"Browser", "Main"})
    public void testMenuDataSaverPersistent() {
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        // Data Saver hasn't been turned on, the footer shouldn't show.
        assertEquals(0, mAppMenuHandler.getDelegate().getFooterResourceId());

        // Turn Data Saver on, the footer should show.
        DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                getActivity().getApplicationContext(), true);
        assertEquals(R.layout.data_reduction_main_menu_footer,
                mAppMenuHandler.getDelegate().getFooterResourceId());

        // Ensure the footer remains if the proxy is turned off.
        DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                getActivity().getApplicationContext(), false);
        assertEquals(R.layout.data_reduction_main_menu_footer,
                mAppMenuHandler.getDelegate().getFooterResourceId());
    }
}
