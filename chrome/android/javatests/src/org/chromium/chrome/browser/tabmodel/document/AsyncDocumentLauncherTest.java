// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
//import android.test.suitebuilder.annotation.MediumTest;
import android.text.TextUtils;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.browser.document.DocumentModeTestBase;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Tests the functionality of the AsyncDocumentLauncher.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
@DisableInTabbedMode
public class AsyncDocumentLauncherTest extends DocumentModeTestBase {
    // @MediumTest
    @DisabledTest
    public void testLaunchingMultipleUnparented() throws Exception {
        AsyncTabCreationParams initialParams = new AsyncTabCreationParams(new LoadUrlParams(URL_1));
        AsyncTabCreationParams secondParams = new AsyncTabCreationParams(new LoadUrlParams(URL_2));
        AsyncTabCreationParams finalParams = new AsyncTabCreationParams(new LoadUrlParams(URL_3));

        AsyncDocumentLauncher.getInstance().enqueueLaunch(false, Tab.INVALID_TAB_ID, initialParams);
        AsyncDocumentLauncher.getInstance().enqueueLaunch(false, Tab.INVALID_TAB_ID, secondParams);
        AsyncDocumentLauncher.getInstance().enqueueLaunch(false, Tab.INVALID_TAB_ID, finalParams);

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!(lastActivity instanceof DocumentActivity)) return false;

                DocumentActivity documentActivity = (DocumentActivity) lastActivity;
                if (documentActivity.getActivityTab() == null) return false;

                return TextUtils.equals(URL_3, documentActivity.getActivityTab().getUrl());
            }
        }));

        TabModelSelector selector = ChromeApplication.getDocumentTabModelSelector();
        assertEquals(3, selector.getTotalTabCount());
        assertEquals(URL_1, selector.getModel(false).getTabAt(0).getUrl());
        assertEquals(URL_2, selector.getModel(false).getTabAt(1).getUrl());
        assertEquals(URL_3, selector.getModel(false).getTabAt(2).getUrl());
    }

    // @MediumTest
    @DisabledTest
    public void testLaunchingMultipleParented() throws Exception {
        // Create an Activity that will be credited with creating the child Activities.
        int parentId = launchViaViewIntent(false, URL_1, "Page 1");

        AsyncTabCreationParams initialParams = new AsyncTabCreationParams(new LoadUrlParams(URL_2));
        AsyncTabCreationParams secondParams = new AsyncTabCreationParams(new LoadUrlParams(URL_3));
        AsyncTabCreationParams finalParams = new AsyncTabCreationParams(new LoadUrlParams(URL_4));

        AsyncDocumentLauncher.getInstance().enqueueLaunch(false, parentId, initialParams);
        AsyncDocumentLauncher.getInstance().enqueueLaunch(false, parentId, secondParams);
        AsyncDocumentLauncher.getInstance().enqueueLaunch(false, parentId, finalParams);

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!(lastActivity instanceof DocumentActivity)) return false;

                DocumentActivity documentActivity = (DocumentActivity) lastActivity;
                if (documentActivity.getActivityTab() == null) return false;

                return TextUtils.equals(URL_4, documentActivity.getActivityTab().getUrl());
            }
        }));

        TabModelSelector selector = ChromeApplication.getDocumentTabModelSelector();
        assertEquals(4, selector.getTotalTabCount());
        assertEquals(URL_1, selector.getModel(false).getTabAt(0).getUrl());
        assertEquals(URL_2, selector.getModel(false).getTabAt(1).getUrl());
        assertEquals(URL_3, selector.getModel(false).getTabAt(2).getUrl());
        assertEquals(URL_4, selector.getModel(false).getTabAt(3).getUrl());
    }

    // @MediumTest
    @DisabledTest
    public void testFailedLaunch() throws Exception {
        // Bloat up the parent Intent so that launching the child Activity will fail.
        int parentId = launchViaViewIntent(false, URL_1, "Page 1");
        final Activity parentActivity = ActivityDelegate.getActivityForTabId(parentId);
        Intent behemothIntent = new Intent(parentActivity.getIntent());
        String fiftyCharString = "01234567890123456789012345678901234567890123456789";
        for (int i = 0; i < 25000; i++) {
            behemothIntent.putExtra("key_" + i, fiftyCharString);
        }
        parentActivity.setIntent(behemothIntent);

        AsyncTabCreationParams initialParams = new AsyncTabCreationParams(new LoadUrlParams(URL_2));
        AsyncTabCreationParams secondParams = new AsyncTabCreationParams(new LoadUrlParams(URL_3));
        AsyncTabCreationParams finalParams = new AsyncTabCreationParams(new LoadUrlParams(URL_4));

        AsyncDocumentLauncher.getInstance().enqueueLaunch(false, Tab.INVALID_TAB_ID, initialParams);
        AsyncDocumentLauncher.getInstance().enqueueLaunch(false, parentId, secondParams);
        AsyncDocumentLauncher.getInstance().enqueueLaunch(false, Tab.INVALID_TAB_ID, finalParams);

        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!(lastActivity instanceof DocumentActivity)) return false;

                DocumentActivity documentActivity = (DocumentActivity) lastActivity;
                if (documentActivity.getActivityTab() == null) return false;

                return TextUtils.equals(URL_4, documentActivity.getActivityTab().getUrl());
            }
        }));

        TabModelSelector selector = ChromeApplication.getDocumentTabModelSelector();
        assertEquals(3, selector.getTotalTabCount());
        assertEquals(URL_1, selector.getModel(false).getTabAt(0).getUrl());
        assertEquals(URL_2, selector.getModel(false).getTabAt(1).getUrl());
        assertEquals(URL_4, selector.getModel(false).getTabAt(2).getUrl());
    }
}
