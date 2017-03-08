// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Test suite for quota permissions requests.
 */
@RetryOnFailure
public class QuotaTest extends PermissionTestCaseBase {
    private static final String TEST_FILE = "/content/test/data/android/quota_permissions.html";

    public QuotaTest() {}

    private void testQuotaPermissionsPlumbing(String script, int numUpdates, boolean withGesture,
            boolean isDialog, boolean hasSwitch, boolean toggleSwitch) throws Exception {
        Tab tab = getActivity().getActivityTab();
        PermissionUpdateWaiter updateWaiter = new PermissionUpdateWaiter("Count: ");
        tab.addObserver(updateWaiter);
        runAllowTest(updateWaiter, TEST_FILE, script, numUpdates, withGesture, isDialog, hasSwitch,
                toggleSwitch);
        tab.removeObserver(updateWaiter);
    }

    /**
     * Verify asking for quota creates an InfoBar and accepting it resolves the call successfully.
     * @throws Exception
     */
    @MediumTest
    @Feature({"QuotaPermissions"})
    @CommandLineFlags.Add({"enable-features=" + PERMISSION_REQUEST_MANAGER_FLAG})
    public void testQuotaShowsInfobar() throws Exception {
        testQuotaPermissionsPlumbing("initiate_requestQuota(1024)", 1, false, false, false, false);
    }
}
