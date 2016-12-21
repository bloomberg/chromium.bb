// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.filters.SmallTest;

import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory;
import org.chromium.content.browser.test.NativeLibraryTestBase;

import java.io.File;

/**
 * Tests whether TabState can be saved and restored to disk properly. Also checks to see if
 * TabStates from previous versions of Chrome can still be loaded and upgraded.
 */
public class TabStateTest extends NativeLibraryTestBase {
    private TestTabModelDirectory mTestTabModelDirectory;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();
        mTestTabModelDirectory = new TestTabModelDirectory(
                getInstrumentation().getTargetContext(), "TabStateTest", null);
    }

    @Override
    public void tearDown() throws Exception {
        TabState.setChannelNameOverrideForTest(null);
        mTestTabModelDirectory.tearDown();
        super.tearDown();
    }

    private void loadAndCheckTabState(TestTabModelDirectory.TabStateInfo info) throws Exception {
        mTestTabModelDirectory.writeTabStateFile(info);

        File tabStateFile = new File(mTestTabModelDirectory.getBaseDirectory(), info.filename);
        TabState tabState = TabState.restoreTabState(tabStateFile, false);
        assertNotNull(tabState);
        assertEquals(info.url, tabState.getVirtualUrlFromState());
        assertEquals(info.title, tabState.getDisplayTitleFromState());
        assertEquals(info.version, tabState.contentsState.version());
    }

    @SmallTest
    public void testLoadV0Tabs() throws Exception {
        TabState.setChannelNameOverrideForTest("stable");
        loadAndCheckTabState(TestTabModelDirectory.M18_GOOGLE_COM);
        loadAndCheckTabState(TestTabModelDirectory.M18_NTP);
    }

    @SmallTest
    public void testLoadV1Tabs() throws Exception {
        TabState.setChannelNameOverrideForTest(null);
        loadAndCheckTabState(TestTabModelDirectory.M26_GOOGLE_COM);
        loadAndCheckTabState(TestTabModelDirectory.M26_GOOGLE_CA);
    }

    @SmallTest
    public void testLoadV2Tabs() throws Exception {
        TabState.setChannelNameOverrideForTest(null);

        // Standard English tabs.
        loadAndCheckTabState(TestTabModelDirectory.V2_DUCK_DUCK_GO);
        loadAndCheckTabState(TestTabModelDirectory.V2_TEXTAREA);

        // Chinese characters.
        loadAndCheckTabState(TestTabModelDirectory.V2_BAIDU);

        // Hebrew, RTL.
        loadAndCheckTabState(TestTabModelDirectory.V2_HAARETZ);
    }
}
