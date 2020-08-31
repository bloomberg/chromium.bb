// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.browser.tabmodel.TestTabModelDirectory;
import org.chromium.chrome.test.ChromeBrowserTestRule;

import java.io.File;

/**
 * Tests whether TabState can be saved and restored to disk properly. Also checks to see if
 * TabStates from previous versions of Chrome can still be loaded and upgraded.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class TabStateTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private TestTabModelDirectory mTestTabModelDirectory;

    @Before
    public void setUp() {
        mTestTabModelDirectory = new TestTabModelDirectory(
                InstrumentationRegistry.getTargetContext(), "TabStateTest", null);
    }

    @After
    public void tearDown() {
        TabState.setChannelNameOverrideForTest(null);
        mTestTabModelDirectory.tearDown();
    }

    private void loadAndCheckTabState(TestTabModelDirectory.TabStateInfo info) throws Exception {
        mTestTabModelDirectory.writeTabStateFile(info);

        File tabStateFile = new File(mTestTabModelDirectory.getBaseDirectory(), info.filename);
        TabState tabState = TabState.restoreTabState(tabStateFile, false);
        Assert.assertNotNull(tabState);
        Assert.assertEquals(info.url, tabState.getVirtualUrlFromState());
        Assert.assertEquals(info.title, tabState.getDisplayTitleFromState());
        Assert.assertEquals(info.version, tabState.contentsState.version());
    }

    @Test
    @SmallTest
    public void testLoadV0Tabs() throws Exception {
        TabState.setChannelNameOverrideForTest("stable");
        loadAndCheckTabState(TestTabModelDirectory.M18_GOOGLE_COM);
        loadAndCheckTabState(TestTabModelDirectory.M18_NTP);
    }

    @Test
    @SmallTest
    public void testLoadV1Tabs() throws Exception {
        TabState.setChannelNameOverrideForTest(null);
        loadAndCheckTabState(TestTabModelDirectory.M26_GOOGLE_COM);
        loadAndCheckTabState(TestTabModelDirectory.M26_GOOGLE_CA);
    }

    @Test
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
