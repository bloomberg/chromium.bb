// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_PHONE;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.widget.BottomSheet;
import org.chromium.chrome.browser.widget.BottomSheet.BottomSheetContent;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;

/**
 * Base class for instrumentation tests using the bottom sheet.
 */
@CommandLineFlags.Add({"enable-features=ChromeHome"})
@Restriction(RESTRICTION_TYPE_PHONE) // ChromeHome is only enabled on phones
public abstract class BottomSheetTestCaseBase extends ChromeTabbedActivityTestBase {
    private boolean mOldChromeHomeFlagValue;
    @Override
    protected void setUp() throws Exception {
        // TODO(dgn,mdjones): Chrome restarts when the ChromeHome feature flag value changes. That
        // crashes the test so we need to manually set the preference to match the flag before
        // Chrome initialises via super.setUp()
        ChromePreferenceManager prefManager = ChromePreferenceManager.getInstance();
        mOldChromeHomeFlagValue = prefManager.isChromeHomeEnabled();
        prefManager.setChromeHomeEnabled(true);

        super.setUp();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().getBottomSheet().setSheetState(
                        BottomSheet.SHEET_STATE_FULL, /* animate = */ false);
            }
        });
        RecyclerViewTestUtils.waitForStableRecyclerView(
                getBottomSheetContent().getScrollingContentView());
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        ChromePreferenceManager.getInstance().setChromeHomeEnabled(mOldChromeHomeFlagValue);
    }

    protected BottomSheetContent getBottomSheetContent() {
        return getActivity().getBottomSheet().getCurrentSheetContent();
    }
}
