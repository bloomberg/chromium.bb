// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.test.suitebuilder.annotation.LargeTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.ContextMenu;
import android.view.KeyEvent;

import junit.framework.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.download.DownloadTestBase;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content.browser.test.util.TestTouchUtils;

import java.io.IOException;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Context menu related tests
 */
@CommandLineFlags.Add(ChromeSwitches.GOOGLE_BASE_URL + "=http://example.com/")
public class ContextMenuTest extends DownloadTestBase {
    private static final String TEST_URL = TestHttpServerClient.getUrl(
            "chrome/test/data/android/contextmenu/context_menu_test.html");

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityWithURL(TEST_URL);
        assertWaitForPageScaleFactorMatch(0.5f);
    }

    @MediumTest
    @Feature({"Browser", "Main"})
    public void testCopyLinkURL() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, "testLink",
                R.id.contextmenu_copy_link_address);

        assertStringContains("test_link.html", getClipboardText());
    }

    @MediumTest
    @Feature({"Browser"})
    public void testCopyImageLinkCopiesLinkURL() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, "testImageLink",
                R.id.contextmenu_copy_link_address);

        assertStringContains("test_link.html", getClipboardText());
    }

    @MediumTest
    @Feature({"Browser"})
    public void testCopyLinkTextSimple() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, "testLink",
                R.id.contextmenu_copy_link_text);

        assertEquals("Clipboard text was not what was expected", "Test Link",
                getClipboardText());
    }

    @MediumTest
    @Feature({"Browser"})
    public void testCopyLinkTextComplex() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, "copyLinkTextComplex",
                R.id.contextmenu_copy_link_text);

        assertEquals("Clipboard text was not what was expected",
                "This is pretty extreme \n(newline). ", getClipboardText());
    }

    @MediumTest
    @Feature({"Browser"})
    public void testLongPressOnImage() throws InterruptedException, TimeoutException {
        final Tab tab = getActivity().getActivityTab();

        TestCallbackHelperContainer helper =
                new TestCallbackHelperContainer(tab.getContentViewCore());

        OnPageFinishedHelper callback = helper.getOnPageFinishedHelper();
        int callbackCount = callback.getCallCount();

        ContextMenuUtils.selectContextMenuItem(this, tab, "testImage",
                R.id.contextmenu_open_image);

        callback.waitForCallback(callbackCount);

        String expectedUrl = TestHttpServerClient.getUrl(
                "chrome/test/data/android/contextmenu/test_image.png");

        String actualUrl = ThreadUtils.runOnUiThreadBlockingNoException(new Callable<String>() {
            @Override
            public String call() throws Exception {
                return tab.getUrl();
            }
        });

        assertEquals("Failed to navigate to the image", expectedUrl, actualUrl);
    }

    @MediumTest
    @Feature({"Browser"})
    public void testLongPressOnImageLink() throws InterruptedException, TimeoutException {
        final Tab tab = getActivity().getActivityTab();

        TestCallbackHelperContainer helper =
                new TestCallbackHelperContainer(tab.getContentViewCore());

        OnPageFinishedHelper callback = helper.getOnPageFinishedHelper();
        int callbackCount = callback.getCallCount();

        ContextMenuUtils.selectContextMenuItem(this, tab, "testImage",
                R.id.contextmenu_open_image);

        callback.waitForCallback(callbackCount);

        String actualTitle = ThreadUtils.runOnUiThreadBlockingNoException(new Callable<String>() {
            @Override
            public String call() throws Exception {
                return tab.getTitle();
            }
        });

        assertTrue("Navigated to the wrong page.", actualTitle.startsWith("test_image.png"));
    }

    @MediumTest
    @Feature({"Browser"})
    public void testDismissContextMenuOnBack() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();
        ContextMenu menu = ContextMenuUtils.openContextMenu(this, tab, "testImage");
        assertNotNull("Context menu was not properly created", menu);
        assertFalse("Context menu did not have window focus", getActivity().hasWindowFocus());

        getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        Assert.assertTrue("Activity did not regain focus.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return getActivity().hasWindowFocus();
                    }
                }));
    }

    @MediumTest
    @Feature({"Browser"})
    public void testDismissContextMenuOnClick() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();
        ContextMenu menu = ContextMenuUtils.openContextMenu(this, tab, "testImage");
        assertNotNull("Context menu was not properly created", menu);
        assertFalse("Context menu did not have window focus", getActivity().hasWindowFocus());

        TestTouchUtils.singleClickView(getInstrumentation(), tab.getView(), 0, 0);

        Assert.assertTrue("Activity did not regain focus.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return getActivity().hasWindowFocus();
                    }
                }));
    }

    @MediumTest
    @Feature({"Browser"})
    public void testCopyEmailAddress() throws InterruptedException, TimeoutException {
        Tab tab = getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, "testEmail",
                R.id.contextmenu_copy_email_address);

        assertEquals("Copied email address is not correct", "someone@example.com",
                getClipboardText());
    }

    @LargeTest
    @Feature({"Browser"})
    public void testSaveDataUrl()
            throws InterruptedException, TimeoutException, SecurityException, IOException {
        saveMediaFromContextMenu("dataUrlIcon", R.id.contextmenu_save_image, "download.gif");
    }

    @LargeTest
    @Feature({"Browser"})
    public void testSaveImage()
            throws InterruptedException, TimeoutException, SecurityException, IOException {
        saveMediaFromContextMenu("testImage", R.id.contextmenu_save_image, "test_image.png");
    }

    /*
     * Long-pressing on a video tag doesn't show a context menu, so this test fails.
     * Bug: http://crbug.com/514745
     *
     * @LargeTest
     * @Feature({"Browser"})
     */
    @DisabledTest
    public void testSaveVideo()
            throws InterruptedException, TimeoutException, SecurityException, IOException {
        saveMediaFromContextMenu("videoDOMElement", R.id.contextmenu_save_video, "test.mp4");
    }

    /**
     * Opens a link and image in new tabs and verifies the order of the tabs. Also verifies that
     * the parent page remains in front after opening links in new tabs.
     *
     * This test only applies in tabbed mode. In document mode, Android handles the ordering of the
     * tabs.
     */
    @LargeTest
    @Feature({"Browser"})
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_DOCUMENT_MODE)
    public void testOpenLinksInNewTabsAndVerifyTabIndexOrdering()
            throws InterruptedException, TimeoutException {
        TabModel tabModel = getActivity().getCurrentTabModel();
        int numOpenedTabs = tabModel.getCount();
        Tab tab = getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, "testLink",
                R.id.contextmenu_open_in_new_tab);
        getInstrumentation().waitForIdleSync();
        int indexOfLinkPage = numOpenedTabs;
        numOpenedTabs += 1;
        assertEquals("Number of open tabs does not match", numOpenedTabs , tabModel.getCount());

        // Wait for any new tab animation to finish if we're being driven by the compositor.
        final LayoutManager layoutDriver = getActivity()
                .getCompositorViewHolder().getLayoutManager();
        assertTrue("Background tab animation not finished.",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return layoutDriver.getActiveLayout().shouldDisplayContentOverlay();
                    }
                }));

        ContextMenuUtils.selectContextMenuItem(this, tab, "testLink2",
                R.id.contextmenu_open_in_new_tab);
        getInstrumentation().waitForIdleSync();
        int indexOfLinkPage2 = numOpenedTabs;
        numOpenedTabs += 1;
        assertEquals("Number of open tabs does not match", numOpenedTabs, tabModel.getCount());

        // Verify the Url is still the same of Parent page.
        assertEquals(TEST_URL, getActivity().getActivityTab().getUrl());

        // Verify that the background tabs were opened in the expected order.
        String newTabUrl = TestHttpServerClient.getUrl(
                "chrome/test/data/android/contextmenu/test_link.html");
        assertEquals(newTabUrl, tabModel.getTabAt(indexOfLinkPage).getUrl());

        String imageUrl = TestHttpServerClient.getUrl(
                "chrome/test/data/android/contextmenu/test_link2.html");
        assertEquals(imageUrl, tabModel.getTabAt(indexOfLinkPage2).getUrl());
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

    private String getClipboardText() {
        ClipboardManager clipMgr =
                (ClipboardManager) getActivity().getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData clipData = clipMgr.getPrimaryClip();
        assertNotNull("Primary clip is null", clipData);
        assertTrue("Primary clip contains no items.", clipData.getItemCount() > 0);
        return clipData.getItemAt(0).getText().toString();
    }

    private void assertStringContains(String subString, String superString) {
        assertTrue("String '" + superString + "' does not contain '" + subString + "'",
                superString.contains(subString));
    }
}
