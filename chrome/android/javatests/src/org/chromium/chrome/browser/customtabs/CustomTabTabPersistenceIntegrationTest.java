// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.support.test.filters.MediumTest;

import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.io.File;
import java.util.Locale;

/**
 * Integration testing for the CustomTab Tab persistence logic.
 */
public class CustomTabTabPersistenceIntegrationTest extends CustomTabActivityTestBase {

    @Override
    public void startMainActivity() throws InterruptedException {
        super.startMainActivity();
        startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                getInstrumentation().getTargetContext(), UrlConstants.ABOUT_BLANK_DISPLAY_URL));
    }

    @MediumTest
    public void testTabFilesDeletedOnClose() {
        Tab tab = getActivity().getActivityTab();
        String expectedTabFileName = TabState.getTabStateFilename(tab.getId(), false);
        String expectedMetadataFileName =
                getActivity().getTabPersistencePolicyForTest().getStateFileName();

        File stateDir = getActivity().getTabPersistencePolicyForTest().getOrCreateStateDirectory();
        waitForFileExistState(true, expectedTabFileName, stateDir);
        waitForFileExistState(true, expectedMetadataFileName, stateDir);

        getActivity().finishAndClose(false);

        waitForFileExistState(false, expectedTabFileName, stateDir);
        waitForFileExistState(false, expectedMetadataFileName, stateDir);
    }

    private void waitForFileExistState(
            final boolean exists, final String fileName, final File filePath) {
        CriteriaHelper.pollInstrumentationThread(new Criteria(
                String.format(Locale.US, "File, %s, expected to exist: %b", fileName, exists)) {
            @Override
            public boolean isSatisfied() {
                File file = new File(filePath, fileName);
                return file.exists() == exists;
            }
        });
    }

}
