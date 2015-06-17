// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.test.FlakyTest;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.CompositorChromeActivity;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.io.IOException;
import java.util.concurrent.TimeoutException;

/**
 * Context menu related tests
 */
@CommandLineFlags.Add(ChromeSwitches.GOOGLE_BASE_URL + "=http://example.com/")
public class ContextMenuTest extends DownloadTestBase {
    private static final String TEST_URL =
            TestHttpServerClient.getUrl("chrome/test/data/android/context_menu_test.html");

    @Override
    public void setUp() throws Exception {
        super.setUp();
        loadUrl(TEST_URL);
        assertWaitForPageScaleFactorMatch(0.5f);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityFromLauncher();
    }

    /*
     * Bug: http://crbug.com/332154
     * @LargeTest
     * @Feature({"Browser"})
     */
    @FlakyTest
    public void testSaveDataUrl()
            throws InterruptedException, TimeoutException, SecurityException, IOException {
        saveMediaFromContextMenu("dataUrlIcon", R.id.contextmenu_save_image, "download.gif");
    }

    /*
     * Bug: http://crbug.com/332154
     * @LargeTest
     * @Feature({"Browser"})
     */
    @FlakyTest
    public void testSaveImage()
            throws InterruptedException, TimeoutException, SecurityException, IOException {
        saveMediaFromContextMenu("testImage", R.id.contextmenu_save_image, "google.png");
    }

    /*
     * Bug: http://crbug.com/332154
     * @LargeTest
     * @Feature({"Browser"})
     */
    @FlakyTest
    public void testSaveVideo()
            throws InterruptedException, TimeoutException, SecurityException, IOException {
        saveMediaFromContextMenu("videoDOMElement", R.id.contextmenu_save_video, "test.mp4");
    }

    // Open link and image in new tabs and Verify the order of indices of the tabs in the TabModel.
    // Also verify, that the parent page remains in front after opening links in new tabs.
    /*
     * Bug: http://crbug.com/412816
     * @LargeTest
     * @Feature({"Browser"})
     */
    @FlakyTest
    public void testOpenLinksInNewTabsAndVerifyTabIndexOrdering()
            throws InterruptedException, TimeoutException {
        int nOpenedTabs = getActivity().getCurrentTabModel().getCount();
        Tab tab = getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, "testLink",
                R.id.contextmenu_open_in_new_tab);
        getInstrumentation().waitForIdleSync();
        int indexOfLinkPage = nOpenedTabs;
        nOpenedTabs += 1;
        assertEquals("Number of open tabs does not match",
                nOpenedTabs , getActivity().getCurrentTabModel().getCount());

        // Wait for any new tab animation to finish if we're being driven by the compositor.
        if (getActivity() instanceof CompositorChromeActivity) {
            final LayoutManager layoutDriver = ((CompositorChromeActivity) getActivity())
                    .getCompositorViewHolder().getLayoutManager();
            assertTrue("Background tab animation not finished.",
                    CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                        @Override
                        public boolean isSatisfied() {
                            return layoutDriver.getActiveLayout().shouldDisplayContentOverlay();
                        }
                    }));
        }

        ContextMenuUtils.selectContextMenuItem(this, tab, "testImage",
                R.id.contextmenu_open_image_in_new_tab);
        getInstrumentation().waitForIdleSync();
        int indexOfLogoPage = nOpenedTabs;
        nOpenedTabs += 1;
        assertEquals("Number of open tabs does not match",
                nOpenedTabs, getActivity().getCurrentTabModel().getCount());

        // Verify the Url is still the same of Parent page.
        assertEquals(TEST_URL, getActivity().getActivityTab().getUrl());

        // Switch to child tab page and verify Url.
        ChromeTabUtils.switchTabInCurrentTabModel(getActivity(), indexOfLinkPage);
        String newTabUrl = TestHttpServerClient.getUrl("chrome/test/data/android/about.html");
        assertEquals(newTabUrl, getActivity().getActivityTab().getUrl());

        // Switch to another child tab page and verify Url.
        ChromeTabUtils.switchTabInCurrentTabModel(getActivity(), indexOfLogoPage);
        String logoTabUrl = TestHttpServerClient.getUrl("chrome/test/data/android/google.png");
        assertEquals(logoTabUrl, getActivity().getActivityTab().getUrl());
    }

    private void saveMediaFromContextMenu(String mediaDOMElement, int saveMenuID,
            String expectedFilename) throws InterruptedException, TimeoutException,
            SecurityException, IOException {
        // Select "save [image/video]" in that menu.
        Tab tab = getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, mediaDOMElement, saveMenuID);

        // Wait for the download to complete and see if we got the right file
        assertTrue(waitForChromeDownloadToFinish());
        checkLastDownload(expectedFilename);
    }
}
