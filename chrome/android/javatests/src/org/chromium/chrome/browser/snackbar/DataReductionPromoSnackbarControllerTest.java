// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar;

import android.support.test.filters.MediumTest;
import android.test.UiThreadTest;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionPromoUtils;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionProxyUma;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;

/**
 * Tests the DataReductionPromoSnackbarController. Tests that the snackbar sizes are properly set
 * from a field trial param and that the correct uma is recorded.
 */
public class DataReductionPromoSnackbarControllerTest extends ChromeTabbedActivityTestBase {

    private static final int BYTES_IN_MB = 1024 * 1024;
    private static final int FIRST_SNACKBAR_SIZE_MB = 100;
    private static final int SECOND_SNACKBAR_SIZE_MB = 1024;
    private static final int COMMAND_LINE_FLAG_SNACKBAR_SIZE_MB = 1;
    private static final String FIRST_SNACKBAR_SIZE_STRING = "100 MB";
    private static final String SECOND_SNACKBAR_SIZE_STRING = "1 GB";
    private static final String COMMAND_LINE_FLAG_SNACKBAR_SIZE_STRING = "1 MB";

    private SnackbarManager mManager;
    private DataReductionPromoSnackbarController mController;

    @Override
    public void startMainActivity() throws InterruptedException {
        ContextUtils.getAppSharedPreferences().edit().clear().apply();
        SnackbarManager.setDurationForTesting(1000);
        startMainActivityOnBlankPage();
        mManager = getActivity().getSnackbarManager();
        mController = new DataReductionPromoSnackbarController(getActivity(), mManager);
    }

    @UiThreadTest
    @MediumTest
    @CommandLineFlags.Add({
            "force-fieldtrials=" + DataReductionPromoSnackbarController.PROMO_FIELD_TRIAL_NAME
                    + "/Enabled",
            "force-fieldtrial-params=" + DataReductionPromoSnackbarController.PROMO_FIELD_TRIAL_NAME
                    + ".Enabled:"
                    + DataReductionPromoSnackbarController.PROMO_PARAM_NAME + "/"
                    + FIRST_SNACKBAR_SIZE_MB + ";"
                    + SECOND_SNACKBAR_SIZE_MB })
    public void testDataReductionPromoSnackbarController() {
        assertFalse(DataReductionPromoUtils.hasSnackbarPromoBeenInitWithStartingSavedBytes());

        mController.maybeShowDataReductionPromoSnackbar(0);

        assertFalse(mManager.isShowing());
        assertTrue(DataReductionPromoUtils.hasSnackbarPromoBeenInitWithStartingSavedBytes());
        assertEquals(0, DataReductionPromoUtils.getDisplayedSnackbarPromoSavedBytes());

        mController.maybeShowDataReductionPromoSnackbar(FIRST_SNACKBAR_SIZE_MB * BYTES_IN_MB);

        assertTrue(mManager.isShowing());
        assertTrue(mManager.getCurrentSnackbarForTesting().getText().toString()
                .endsWith(FIRST_SNACKBAR_SIZE_STRING));
        assertEquals(FIRST_SNACKBAR_SIZE_MB * BYTES_IN_MB,
                DataReductionPromoUtils.getDisplayedSnackbarPromoSavedBytes());
        mManager.dismissSnackbars(mController);

        mController.maybeShowDataReductionPromoSnackbar(SECOND_SNACKBAR_SIZE_MB * BYTES_IN_MB);

        assertTrue(mManager.isShowing());

        assertTrue(mManager.getCurrentSnackbarForTesting().getText().toString()
                .endsWith(SECOND_SNACKBAR_SIZE_STRING));
        assertEquals(SECOND_SNACKBAR_SIZE_MB * BYTES_IN_MB,
                DataReductionPromoUtils.getDisplayedSnackbarPromoSavedBytes());
    }

    @UiThreadTest
    @MediumTest
    @CommandLineFlags.Add({
            "force-fieldtrials=" + DataReductionPromoSnackbarController.PROMO_FIELD_TRIAL_NAME
                    + "/Enabled",
            "force-fieldtrial-params=" + DataReductionPromoSnackbarController.PROMO_FIELD_TRIAL_NAME
                    + ".Enabled:"
                    + DataReductionPromoSnackbarController.PROMO_PARAM_NAME + "/"
                    + FIRST_SNACKBAR_SIZE_MB + ";"
                    + SECOND_SNACKBAR_SIZE_MB })
    public void testDataReductionPromoSnackbarControllerExistingUser() {
        assertFalse(DataReductionPromoUtils.hasSnackbarPromoBeenInitWithStartingSavedBytes());

        mController.maybeShowDataReductionPromoSnackbar(SECOND_SNACKBAR_SIZE_MB * BYTES_IN_MB);

        assertFalse(mManager.isShowing());
        assertTrue(DataReductionPromoUtils.hasSnackbarPromoBeenInitWithStartingSavedBytes());
        assertEquals(SECOND_SNACKBAR_SIZE_MB * BYTES_IN_MB,
                DataReductionPromoUtils.getDisplayedSnackbarPromoSavedBytes());

        mController.maybeShowDataReductionPromoSnackbar(SECOND_SNACKBAR_SIZE_MB * BYTES_IN_MB + 1);

        assertFalse(mManager.isShowing());
        assertEquals(SECOND_SNACKBAR_SIZE_MB * BYTES_IN_MB,
                DataReductionPromoUtils.getDisplayedSnackbarPromoSavedBytes());
    }

    @UiThreadTest
    @MediumTest
    @CommandLineFlags.Add({
            "force-fieldtrials=" + DataReductionPromoSnackbarController.PROMO_FIELD_TRIAL_NAME
                    + "/Enabled",
            "force-fieldtrial-params=" + DataReductionPromoSnackbarController.PROMO_FIELD_TRIAL_NAME
                    + ".Enabled:"
                    + DataReductionPromoSnackbarController.PROMO_PARAM_NAME + "/"
                    + FIRST_SNACKBAR_SIZE_MB + ";"
                    + SECOND_SNACKBAR_SIZE_MB })
    public void testDataReductionPromoSnackbarControllerHistograms() {
        assertFalse(DataReductionPromoUtils.hasSnackbarPromoBeenInitWithStartingSavedBytes());

        mController.maybeShowDataReductionPromoSnackbar(0);

        assertTrue(DataReductionPromoUtils.hasSnackbarPromoBeenInitWithStartingSavedBytes());
        assertEquals(0, RecordHistogram
                .getHistogramValueCountForTesting(DataReductionProxyUma.SNACKBAR_HISTOGRAM_NAME,
                        0));

        mController.maybeShowDataReductionPromoSnackbar(FIRST_SNACKBAR_SIZE_MB * BYTES_IN_MB);

        assertTrue(mManager.isShowing());
        assertEquals(1, RecordHistogram
                .getHistogramValueCountForTesting(DataReductionProxyUma.SNACKBAR_HISTOGRAM_NAME,
                        FIRST_SNACKBAR_SIZE_MB));
        mManager.getCurrentSnackbarForTesting().getController().onDismissNoAction(null);
        assertEquals(1, RecordHistogram
                .getHistogramValueCountForTesting(DataReductionProxyUma.UI_ACTION_HISTOGRAM_NAME,
                        DataReductionProxyUma.ACTION_SNACKBAR_DISMISSED));

        mController.maybeShowDataReductionPromoSnackbar(SECOND_SNACKBAR_SIZE_MB * BYTES_IN_MB);

        assertTrue(mManager.isShowing());
        assertEquals(1, RecordHistogram
                .getHistogramValueCountForTesting(DataReductionProxyUma.SNACKBAR_HISTOGRAM_NAME,
                        SECOND_SNACKBAR_SIZE_MB));
        mManager.getCurrentSnackbarForTesting().getController().onAction(null);
        // The dismissed histogram should not have been incremented.
        assertEquals(1, RecordHistogram
                .getHistogramValueCountForTesting(DataReductionProxyUma.UI_ACTION_HISTOGRAM_NAME,
                        DataReductionProxyUma.ACTION_SNACKBAR_DISMISSED));
    }

    @UiThreadTest
    @MediumTest
    @CommandLineFlags.Add({ "enable-data-reduction-proxy-savings-promo" })
    public void testDataReductionPromoSnackbarControllerCommandLineFlag() {
        assertFalse(DataReductionPromoUtils.hasSnackbarPromoBeenInitWithStartingSavedBytes());
        mController.maybeShowDataReductionPromoSnackbar(0);

        mController.maybeShowDataReductionPromoSnackbar(
                COMMAND_LINE_FLAG_SNACKBAR_SIZE_MB * BYTES_IN_MB);

        assertTrue(mManager.isShowing());
        assertTrue(mManager.getCurrentSnackbarForTesting().getText().toString()
                .endsWith(COMMAND_LINE_FLAG_SNACKBAR_SIZE_STRING));
        assertEquals(COMMAND_LINE_FLAG_SNACKBAR_SIZE_MB * BYTES_IN_MB,
                DataReductionPromoUtils.getDisplayedSnackbarPromoSavedBytes());
        mManager.dismissSnackbars(mController);

        mController.maybeShowDataReductionPromoSnackbar(FIRST_SNACKBAR_SIZE_MB * BYTES_IN_MB);

        assertFalse(mManager.isShowing());
    }

    @UiThreadTest
    @MediumTest
    @CommandLineFlags.Add({
            "enable-data-reduction-proxy-savings-promo",
            "force-fieldtrials=" + DataReductionPromoSnackbarController.PROMO_FIELD_TRIAL_NAME
                    + "/Enabled",
            "force-fieldtrial-params=" + DataReductionPromoSnackbarController.PROMO_FIELD_TRIAL_NAME
                    + ".Enabled:"
                    + DataReductionPromoSnackbarController.PROMO_PARAM_NAME + "/"
                    + FIRST_SNACKBAR_SIZE_MB + ";"
                    + SECOND_SNACKBAR_SIZE_MB })
    public void testDataReductionPromoSnackbarControllerCommandLineFlagWithFieldTrial() {
        assertFalse(DataReductionPromoUtils.hasSnackbarPromoBeenInitWithStartingSavedBytes());
        mController.maybeShowDataReductionPromoSnackbar(0);

        mController.maybeShowDataReductionPromoSnackbar(
                COMMAND_LINE_FLAG_SNACKBAR_SIZE_MB * BYTES_IN_MB);

        assertTrue(mManager.isShowing());
        assertTrue(mManager.getCurrentSnackbarForTesting().getText().toString()
                .endsWith(COMMAND_LINE_FLAG_SNACKBAR_SIZE_STRING));
        assertEquals(COMMAND_LINE_FLAG_SNACKBAR_SIZE_MB * BYTES_IN_MB,
                DataReductionPromoUtils.getDisplayedSnackbarPromoSavedBytes());
        mManager.dismissSnackbars(mController);

        mController.maybeShowDataReductionPromoSnackbar(FIRST_SNACKBAR_SIZE_MB * BYTES_IN_MB);

        assertTrue(mManager.isShowing());

        assertTrue(mManager.getCurrentSnackbarForTesting().getText().toString()
                .endsWith(FIRST_SNACKBAR_SIZE_STRING));
        assertEquals(FIRST_SNACKBAR_SIZE_MB * BYTES_IN_MB,
                DataReductionPromoUtils.getDisplayedSnackbarPromoSavedBytes());
    }
}