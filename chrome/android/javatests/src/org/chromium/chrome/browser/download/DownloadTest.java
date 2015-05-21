// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.test.FlakyTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;

/**
 * Tests Chrome download feature by attempting to download some files.
 */
public class DownloadTest extends DownloadTestBase {
    private static final String SUPERBO_CONTENTS =
            "plain text response from a POST";

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    /**
     * Bug http://crbug.com/253711
     *
     * @MediumTest
     * @Feature({"Downloads"})
     */
    @FlakyTest
    public void testHttpGetDownload() throws Exception {
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/download/get.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();

        singleClickView(currentView);
        assertTrue(waitForGetDownloadToFinish());
        checkLastDownload("test.gzip");
    }

    /**
     * Bug http://crbug/286315
     *
     * @MediumTest
     * @Feature({"Downloads"})
     */
    @FlakyTest
    public void testDangerousDownload() throws Exception {
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/download/dangerous.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();
        singleClickView(currentView);
        assertPollForInfoBarSize(1);
        assertTrue("OK button wasn't found", InfoBarUtil.clickPrimaryButton(getInfoBars().get(0)));
        assertTrue(waitForGetDownloadToFinish());
        checkLastDownload("test.apk");
    }

    /**
     * Bug http://crbug/253711
     *
     * @MediumTest
     * @Feature({"Downloads"})
     */
    @FlakyTest
    public void testHttpPostDownload() throws Exception {
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/download/post.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();

        singleClickView(currentView);
        assertTrue(waitForChromeDownloadToFinish());
        assertTrue(hasDownload("superbo.txt", SUPERBO_CONTENTS));
    }

    /**
     * Bug 5431234
     *
     * @MediumTest
     * @Feature({"Downloads"})
     */
    @FlakyTest
    public void testCloseEmptyDownloadTab() throws Exception {
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/download/get.html"));
        waitForFocus();
        int initialTabCount = getActivity().getCurrentTabModel().getCount();
        View currentView = getActivity().getActivityTab().getView();
        TouchCommon.longPressView(currentView, currentView.getWidth() / 2,
                currentView.getHeight() / 2);

        getInstrumentation().invokeContextMenuAction(getActivity(),
                R.id.contextmenu_open_in_new_tab, 0);
        assertTrue(waitForGetDownloadToFinish());
        checkLastDownload("test.gzip");

        assertEquals("Did not close new blank tab for download", initialTabCount,
                getActivity().getCurrentTabModel().getCount());
    }

    /*
    Bug http://crbug/415711
    */
    @MediumTest
    @Feature({"Downloads"})
    public void testDuplicateHttpPostDownload_Overwrite() throws Exception {
        // Download a file.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/download/post.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();
        singleClickView(currentView);
        assertTrue("Failed to finish downloading file for the first time.",
                waitForChromeDownloadToFinish());

        // Download a file with the same name.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/download/post.html"));
        waitForFocus();
        currentView = getActivity().getActivityTab().getView();
        singleClickView(currentView);
        assertPollForInfoBarSize(1);
        assertTrue("OVERWRITE button wasn't found",
                InfoBarUtil.clickPrimaryButton(getInfoBars().get(0)));
        assertTrue("Failed to finish downloading file for the second time.",
                waitForChromeDownloadToFinish());

        assertTrue("Missing first download", hasDownload("superbo.txt", SUPERBO_CONTENTS));
        assertFalse("Should not have second download",
                hasDownload("superbo (1).txt", SUPERBO_CONTENTS));
    }

    /**
     * Bug http://crbug/253711
     * Bug http://crbug/415711
     *
     * @MediumTest
     * @Feature({"Downloads"})
     */
    @FlakyTest
    public void testDuplicateHttpPostDownload_CreateNew() throws Exception {
        // Download a file.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/download/post.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();
        singleClickView(currentView);
        assertTrue("Failed to finish downloading file for the first time.",
                waitForChromeDownloadToFinish());

        // Download a file with the same name.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/download/post.html"));
        waitForFocus();
        currentView = getActivity().getActivityTab().getView();
        singleClickView(currentView);
        assertPollForInfoBarSize(1);
        assertTrue("CREATE NEW button wasn't found",
                InfoBarUtil.clickSecondaryButton(getInfoBars().get(0)));
        assertTrue("Failed to finish downloading file for the second time.",
                waitForChromeDownloadToFinish());

        assertTrue("Missing first download", hasDownload("superbo.txt", SUPERBO_CONTENTS));
        assertTrue("Missing second download",
                hasDownload("superbo (1).txt", SUPERBO_CONTENTS));
    }

    /*
    Bug http://crbug/415711
    */
    @MediumTest
    @Feature({"Downloads"})
    public void testDuplicateHttpPostDownload_Dismiss() throws Exception {
        // Download a file.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/download/post.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();
        singleClickView(currentView);
        assertTrue("Failed to finish downloading file for the first time.",
                waitForChromeDownloadToFinish());

        // Download a file with the same name.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/download/post.html"));
        waitForFocus();
        currentView = getActivity().getActivityTab().getView();
        singleClickView(currentView);
        assertPollForInfoBarSize(1);
        assertTrue("Close button wasn't found",
                InfoBarUtil.clickCloseButton(getInfoBars().get(0)));
        assertFalse(
                "Download should not happen when closing infobar", waitForChromeDownloadToFinish());

        assertTrue("Missing first download", hasDownload("superbo.txt", SUPERBO_CONTENTS));
        assertFalse("Should not have second download",
                hasDownload("superbo (1).txt", SUPERBO_CONTENTS));
    }

    /*
    Bug http://crbug/415711
    */
    @MediumTest
    @Feature({"Downloads"})
    public void testDuplicateHttpPostDownload_AllowMultipleInfoBars() throws Exception {
        assertFalse(hasDownload("superbo.txt", SUPERBO_CONTENTS));
        // Download a file.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/download/post.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();
        singleClickView(currentView);
        assertTrue("Failed to finish downloading file for the first time.",
                waitForChromeDownloadToFinish());

        // Download the file for the second time.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/download/post.html"));
        waitForFocus();
        currentView = getActivity().getActivityTab().getView();
        singleClickView(currentView);
        assertPollForInfoBarSize(1);

        // Download the file for the third time.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/download/post.html"));
        waitForFocus();
        currentView = getActivity().getActivityTab().getView();
        singleClickView(currentView);
        assertPollForInfoBarSize(2);

        // Now create two new files by clicking on the infobars.
        assertTrue("CREATE NEW button wasn't found",
                InfoBarUtil.clickSecondaryButton(getInfoBars().get(0)));
        assertTrue(
                "Failed to finish downloading the second file.", waitForChromeDownloadToFinish());
        assertPollForInfoBarSize(1);
        assertTrue("CREATE NEW button wasn't found",
                InfoBarUtil.clickSecondaryButton(getInfoBars().get(0)));
        assertTrue("Failed to finish downloading the third file.", waitForChromeDownloadToFinish());

        assertTrue("Missing first download", hasDownload("superbo.txt", SUPERBO_CONTENTS));
        assertTrue("Missing second download", hasDownload("superbo (1).txt", SUPERBO_CONTENTS));
        assertTrue("Missing third download", hasDownload("superbo (2).txt", SUPERBO_CONTENTS));
    }

    /*
    @MediumTest
    @Feature({"Downloads"})
    Bug http://crbug/253711
    */
    @FlakyTest
    public void testUrlEscaping() throws Exception {
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/download/urlescaping.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();

        singleClickView(currentView);
        assertTrue(waitForGetDownloadToFinish());
        checkLastDownload("[large]wallpaper.dm");
    }

    private void waitForFocus() {
        View currentView = getActivity().getActivityTab().getView();
        if (!currentView.hasFocus()) {
            singleClickView(currentView);
        }
        getInstrumentation().waitForIdleSync();
    }

    /**
     * Wait until info bar size becomes the given size and the last info bar becomes ready if there
     * is one more more.
     * @param size The size of info bars to poll for.
     */
    private void assertPollForInfoBarSize(final int size) throws InterruptedException {
        assertTrue("There should be " + size + " infobar but there are "
                + getInfoBars().size() + " infobars.",
                CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        if (getInfoBars().size() != size) return false;
                        if (size == 0) return true;
                        InfoBar infoBar = getInfoBars().get(size - 1);
                        return infoBar.areControlsEnabled();
                    }
                }));
    }
}
