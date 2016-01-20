// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.components.offlinepages.FeatureMode;
import org.chromium.content.browser.test.NativeLibraryTestBase;

/** Unit tests for offline pages feature. */
public class OfflinePageFeatureTest extends NativeLibraryTestBase {
    @Override
    protected void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();
    }

    @CommandLineFlags.Add({ChromeSwitches.ENABLE_OFFLINE_PAGES_AS_BOOKMARKS})
    @SmallTest
    @UiThreadTest
    public void testEnableOfflinePagesAsBookmarks() throws Exception {
        assertEquals(FeatureMode.ENABLED_AS_BOOKMARKS, OfflinePageBridge.getFeatureMode());

        /** String is not updated since the experiment switch asks for the "bookmarks" version. */
        assertEquals(
                R.string.menu_bookmarks, OfflinePageUtils.getStringId(R.string.menu_bookmarks));
        /** String is not updated when no mapping exists. */
        assertEquals(R.string.bookmark_offline_page_none,
                OfflinePageUtils.getStringId(R.string.bookmark_offline_page_none));
    }

    @CommandLineFlags.Add({ChromeSwitches.ENABLE_OFFLINE_PAGES_AS_SAVED_PAGES})
    @SmallTest
    @UiThreadTest
    public void testEnableOfflinePagesAsSavedPages() throws Exception {
        assertEquals(FeatureMode.ENABLED_AS_SAVED_PAGES, OfflinePageBridge.getFeatureMode());

        /** String is updated when a mapping is found. */
        assertEquals(R.string.menu_bookmarks_offline_pages,
                OfflinePageUtils.getStringId(R.string.menu_bookmarks));
        /** String is not updated when no mapping exists. */
        assertEquals(R.string.bookmark_offline_page_none,
                OfflinePageUtils.getStringId(R.string.bookmark_offline_page_none));
    }

    @CommandLineFlags.Add({ChromeSwitches.ENABLE_OFFLINE_PAGES})
    @SmallTest
    @UiThreadTest
    public void testEnableOfflinePages() throws Exception {
        assertEquals(FeatureMode.ENABLED_AS_SAVED_PAGES, OfflinePageBridge.getFeatureMode());

        /** String is not updated since the experiment switch asks for the "bookmarks" version. */
        assertEquals(R.string.menu_bookmarks_offline_pages,
                OfflinePageUtils.getStringId(R.string.menu_bookmarks));
        /** String is not updated when no mapping exists. */
        assertEquals(R.string.bookmark_offline_page_none,
                OfflinePageUtils.getStringId(R.string.bookmark_offline_page_none));
    }

    @CommandLineFlags.Add({ChromeSwitches.DISABLE_OFFLINE_PAGES})
    @SmallTest
    @UiThreadTest
    public void testDisableOfflinePages() throws Exception {
        assertEquals(FeatureMode.DISABLED, OfflinePageBridge.getFeatureMode());

        /** Strings are not updated regardless whether the mappings exist when the feature is
         * disabled.
         */
        assertEquals(
                R.string.menu_bookmarks, OfflinePageUtils.getStringId(R.string.menu_bookmarks));
        assertEquals(R.string.bookmark_offline_page_none,
                OfflinePageUtils.getStringId(R.string.bookmark_offline_page_none));
    }
}
