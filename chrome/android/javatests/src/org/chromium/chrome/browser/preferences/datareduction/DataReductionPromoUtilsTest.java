// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.datareduction;

import android.content.Context;
import android.support.test.filters.SmallTest;
import android.test.UiThreadTest;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PrefServiceBridge.AboutVersionStrings;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/**
 * Unit test suite for DataReductionPromoUtils.
 */
public class DataReductionPromoUtilsTest extends NativeLibraryTestBase {
    private static final String SHARED_PREF_DISPLAYED_INFOBAR_PROMO_VERSION =
            "displayed_data_reduction_infobar_promo_version";

    private Context mContext;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();
        // Using an AdvancedMockContext allows us to use a fresh in-memory SharedPreference.
        mContext = new AdvancedMockContext(
                getInstrumentation().getTargetContext().getApplicationContext());
        ContextUtils.initApplicationContextForTests(mContext);
    }

    /**
     * Tests that promos cannot be shown if the data reduction proxy is enabled.
     */
    @UiThreadTest
    @SmallTest
    @CommandLineFlags.Add("force-fieldtrials=DataCompressionProxyPromoVisibility/Enabled")
    @Feature({"DataReduction"})
    public void testCanShowPromos() {
        if (DataReductionProxySettings.getInstance().isDataReductionProxyManaged()) return;
        assertFalse(DataReductionProxySettings.getInstance().isDataReductionProxyEnabled());
        assertTrue(DataReductionPromoUtils.canShowPromos());
        DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(mContext, true);
        assertFalse(DataReductionPromoUtils.canShowPromos());
    }

    /**
     * Tests that saving the first run experience and second run promo state updates the pref and
     * also stores the application version. Tests that the first run experience opt out pref is
     * updated.
     */
    @UiThreadTest
    @SmallTest
    @Feature({"DataReduction"})
    public void testFreOrSecondRunPromoDisplayed() {
        AboutVersionStrings versionStrings = PrefServiceBridge.getInstance()
                .getAboutVersionStrings();

        // The first run experience or second run promo should not have been shown yet.
        assertFalse(DataReductionPromoUtils.getDisplayedFreOrSecondRunPromo());

        // Save that the first run experience or second run promo has been displayed.
        DataReductionPromoUtils.saveFreOrSecondRunPromoDisplayed();
        assertTrue(DataReductionPromoUtils.getDisplayedFreOrSecondRunPromo());
        assertFalse(DataReductionPromoUtils.getDisplayedInfoBarPromo());
        assertFalse(DataReductionPromoUtils.getOptedOutOnFrePromo());
        assertEquals(versionStrings.getApplicationVersion(),
                DataReductionPromoUtils.getDisplayedFreOrSecondRunPromoVersion());
    }

    /**
     * Tests that the first run experience opt out pref is updated.
     */
    @UiThreadTest
    @SmallTest
    @Feature({"DataReduction"})
    public void testFrePromoOptOut() {
        // Save that the user opted out of the first run experience.
        DataReductionPromoUtils.saveFrePromoOptOut(true);
        assertTrue(DataReductionPromoUtils.getOptedOutOnFrePromo());

        // Save that the user did not opt out of the first run experience.
        DataReductionPromoUtils.saveFrePromoOptOut(false);
        assertFalse(DataReductionPromoUtils.getOptedOutOnFrePromo());
    }

    /**
     * Tests that saving the infobar promo state updates the pref and also stores the application
     * version.
     */
    @UiThreadTest
    @SmallTest
    @Feature({"DataReduction"})
    public void testInfoBarPromoDisplayed() {
        AboutVersionStrings versionStrings =
                PrefServiceBridge.getInstance().getAboutVersionStrings();

        // The infobar should not have been shown yet.
        assertFalse(DataReductionPromoUtils.getDisplayedInfoBarPromo());

        // Save that the infobar promo has been displayed.
        DataReductionPromoUtils.saveInfoBarPromoDisplayed();
        assertFalse(DataReductionPromoUtils.getDisplayedFreOrSecondRunPromo());
        assertTrue(DataReductionPromoUtils.getDisplayedInfoBarPromo());
        assertFalse(DataReductionPromoUtils.getOptedOutOnFrePromo());
        assertEquals(versionStrings.getApplicationVersion(), ContextUtils.getAppSharedPreferences()
                .getString(SHARED_PREF_DISPLAYED_INFOBAR_PROMO_VERSION, ""));
    }
}
