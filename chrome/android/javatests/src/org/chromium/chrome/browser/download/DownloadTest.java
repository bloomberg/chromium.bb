// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.os.Environment;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View;

import org.chromium.base.Log;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.File;
import java.util.concurrent.Callable;

/**
 * Tests Chrome download feature by attempting to download some files.
 */
public class DownloadTest extends DownloadTestBase {
    private static final String TAG = "cr_DownloadTest";
    private static final String SUPERBO_CONTENTS =
            "plain text response from a POST";

    private EmbeddedTestServer mTestServer;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @MediumTest
    @Feature({"Downloads"})
    public void testHttpGetDownload() throws Exception {
        loadUrl(mTestServer.getURL("/chrome/test/data/android/download/get.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();

        EnqueueHttpGetDownloadCallbackHelper callbackHelper = getHttpGetDownloadCallbackHelper();
        int callCount = callbackHelper.getCallCount();
        singleClickView(currentView);
        callbackHelper.waitForCallback(callCount);

        assertEquals(mTestServer.getURL("/chrome/test/data/android/download/test.gzip"),
                callbackHelper.getDownloadInfo().getUrl());
    }

    @MediumTest
    @Feature({"Downloads"})
    public void testDangerousDownload() throws Exception {
        loadUrl(mTestServer.getURL("/chrome/test/data/android/download/dangerous.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();
        singleClickView(currentView);
        assertPollForInfoBarSize(1);

        EnqueueHttpGetDownloadCallbackHelper callbackHelper = getHttpGetDownloadCallbackHelper();
        int callCount = callbackHelper.getCallCount();
        assertTrue("OK button wasn't found", InfoBarUtil.clickPrimaryButton(getInfoBars().get(0)));
        callbackHelper.waitForCallback(callCount);
        assertEquals(mTestServer.getURL("/chrome/test/data/android/download/test.apk"),
                callbackHelper.getDownloadInfo().getUrl());
    }

    @MediumTest
    @Feature({"Downloads"})
    public void testHttpPostDownload() throws Exception {
        loadUrl(mTestServer.getURL("/chrome/test/data/android/download/post.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();

        int callCount = getChromeDownloadCallCount();
        singleClickView(currentView);
        assertTrue(waitForChromeDownloadToFinish(callCount));
        assertTrue(hasDownload("superbo.txt", SUPERBO_CONTENTS));
    }

    @MediumTest
    @Feature({"Downloads"})
    @DisabledTest(message = "crbug.com/286315")
    public void testCloseEmptyDownloadTab() throws Exception {
        loadUrl(mTestServer.getURL("/chrome/test/data/android/download/get.html"));
        waitForFocus();
        final int initialTabCount = getActivity().getCurrentTabModel().getCount();
        View currentView = getActivity().getActivityTab().getView();
        TouchCommon.longPressView(currentView);

        EnqueueHttpGetDownloadCallbackHelper callbackHelper = getHttpGetDownloadCallbackHelper();
        int callCount = callbackHelper.getCallCount();
        getInstrumentation().invokeContextMenuAction(getActivity(),
                R.id.contextmenu_open_in_new_tab, 0);
        callbackHelper.waitForCallback(callCount);
        assertEquals(mTestServer.getURL("/chrome/test/data/android/download/test.gzip"),
                callbackHelper.getDownloadInfo().getUrl());

        CriteriaHelper.pollUiThread(
                Criteria.equals(initialTabCount, new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return getActivity().getCurrentTabModel().getCount();
                    }
                }));
    }

    @MediumTest
    @Feature({"Downloads"})
    public void testDuplicateHttpPostDownload_Overwrite() throws Exception {
        // Snackbar overlaps the infobar which is clicked in this test.
        getActivity().getSnackbarManager().disableForTesting();
        // Download a file.
        loadUrl(mTestServer.getURL("/chrome/test/data/android/download/post.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();
        int callCount = getChromeDownloadCallCount();
        singleClickView(currentView);
        assertTrue("Failed to finish downloading file for the first time.",
                waitForChromeDownloadToFinish(callCount));

        // Download a file with the same name.
        loadUrl(mTestServer.getURL("/chrome/test/data/android/download/post.html"));
        waitForFocus();
        currentView = getActivity().getActivityTab().getView();
        callCount = getChromeDownloadCallCount();
        singleClickView(currentView);
        assertPollForInfoBarSize(1);
        assertTrue("OVERWRITE button wasn't found",
                InfoBarUtil.clickPrimaryButton(getInfoBars().get(0)));
        assertTrue("Failed to finish downloading file for the second time.",
                waitForChromeDownloadToFinish(callCount));

        assertTrue("Missing first download", hasDownload("superbo.txt", SUPERBO_CONTENTS));
        assertFalse("Should not have second download",
                hasDownload("superbo (1).txt", SUPERBO_CONTENTS));
    }

    @MediumTest
    @Feature({"Downloads"})
    @DisabledTest(message = "crbug.com/597230")
    public void testDuplicateHttpPostDownload_CreateNew() throws Exception {
        // Download a file.
        loadUrl(mTestServer.getURL("/chrome/test/data/android/download/post.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();
        int callCount = getChromeDownloadCallCount();
        singleClickView(currentView);
        assertTrue("Failed to finish downloading file for the first time.",
                waitForChromeDownloadToFinish(callCount));

        // Download a file with the same name.
        loadUrl(mTestServer.getURL("/chrome/test/data/android/download/post.html"));
        waitForFocus();
        currentView = getActivity().getActivityTab().getView();
        callCount = getChromeDownloadCallCount();
        singleClickView(currentView);
        assertPollForInfoBarSize(1);
        assertTrue("CREATE NEW button wasn't found",
                InfoBarUtil.clickSecondaryButton(getInfoBars().get(0)));
        assertTrue("Failed to finish downloading file for the second time.",
                waitForChromeDownloadToFinish(callCount));

        assertTrue("Missing first download", hasDownload("superbo.txt", SUPERBO_CONTENTS));
        assertTrue("Missing second download",
                hasDownload("superbo (1).txt", SUPERBO_CONTENTS));
    }

    @MediumTest
    @Feature({"Downloads"})
    @FlakyTest(message = "crbug.com/415711")
    public void testDuplicateHttpPostDownload_Dismiss() throws Exception {
        // Download a file.
        loadUrl(mTestServer.getURL("/chrome/test/data/android/download/post.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();
        int callCount = getChromeDownloadCallCount();
        singleClickView(currentView);
        assertTrue("Failed to finish downloading file for the first time.",
                waitForChromeDownloadToFinish(callCount));

        // Download a file with the same name.
        loadUrl(mTestServer.getURL("/chrome/test/data/android/download/post.html"));
        waitForFocus();
        currentView = getActivity().getActivityTab().getView();
        callCount = getChromeDownloadCallCount();
        singleClickView(currentView);
        assertPollForInfoBarSize(1);
        assertTrue("Close button wasn't found",
                InfoBarUtil.clickCloseButton(getInfoBars().get(0)));
        assertFalse("Download should not happen when closing infobar",
                waitForChromeDownloadToFinish(callCount));

        assertTrue("Missing first download", hasDownload("superbo.txt", SUPERBO_CONTENTS));
        assertFalse("Should not have second download",
                hasDownload("superbo (1).txt", SUPERBO_CONTENTS));
    }

    @MediumTest
    @Feature({"Downloads"})
    @DisabledTest(message = "crbug.com/597230")
    public void testDuplicateHttpPostDownload_AllowMultipleInfoBars() throws Exception {
        assertFalse(hasDownload("superbo.txt", SUPERBO_CONTENTS));
        // Download a file.
        loadUrl(mTestServer.getURL("/chrome/test/data/android/download/post.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();
        int callCount = getChromeDownloadCallCount();
        singleClickView(currentView);
        assertTrue("Failed to finish downloading file for the first time.",
                waitForChromeDownloadToFinish(callCount));

        // Download the file for the second time.
        loadUrl(mTestServer.getURL("/chrome/test/data/android/download/post.html"));
        waitForFocus();
        currentView = getActivity().getActivityTab().getView();
        singleClickView(currentView);
        assertPollForInfoBarSize(1);

        // Download the file for the third time.
        loadUrl(mTestServer.getURL("/chrome/test/data/android/download/post.html"));
        waitForFocus();
        currentView = getActivity().getActivityTab().getView();
        singleClickView(currentView);
        assertPollForInfoBarSize(2);

        // Now create two new files by clicking on the infobars.
        callCount = getChromeDownloadCallCount();
        assertTrue("CREATE NEW button wasn't found",
                InfoBarUtil.clickSecondaryButton(getInfoBars().get(0)));
        assertTrue("Failed to finish downloading the second file.",
                waitForChromeDownloadToFinish(callCount));
        assertPollForInfoBarSize(1);
        callCount = getChromeDownloadCallCount();
        assertTrue("CREATE NEW button wasn't found",
                InfoBarUtil.clickSecondaryButton(getInfoBars().get(0)));
        assertTrue("Failed to finish downloading the third file.",
                waitForChromeDownloadToFinish(callCount));

        assertTrue("Missing first download", hasDownload("superbo.txt", SUPERBO_CONTENTS));
        assertTrue("Missing second download", hasDownload("superbo (1).txt", SUPERBO_CONTENTS));
        assertTrue("Missing third download", hasDownload("superbo (2).txt", SUPERBO_CONTENTS));
    }

    private void goToLastTab() throws Exception {
        final TabModel model = getActivity().getCurrentTabModel();
        final int count = model.getCount();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelUtils.setIndex(model, count - 1);
            }
        });

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getActivity().getActivityTab() == model.getTabAt(count - 1)
                        && getActivity().getActivityTab().isReady();
            }
        });
    }

    private void waitForNewTabToStabilize(final int numTabsAfterNewTab)
            throws InterruptedException {
        // Wait until we have a new tab first. This should be called before checking the active
        // layout because the active layout changes StaticLayout --> SimpleAnimationLayout
        // --> (tab added) --> StaticLayout.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                updateFailureReason(
                        "Actual tab count: " + getActivity().getCurrentTabModel().getCount());
                return getActivity().getCurrentTabModel().getCount() >= numTabsAfterNewTab;
            }
        });

        // Now wait until the new tab animation finishes. Something wonky happens
        // if we try to go to the new tab before this.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                CompositorViewHolder compositorViewHolder =
                        (CompositorViewHolder) getActivity().findViewById(
                                R.id.compositor_view_holder);
                LayoutManager layoutManager = compositorViewHolder.getLayoutManager();

                return layoutManager.getActiveLayout() instanceof StaticLayout;
            }
        });
    }

    @CommandLineFlags.Add(ChromeSwitches.DISABLE_DOCUMENT_MODE)
    @DisabledTest(message = "crbug.com/606798")
    @MediumTest
    @Feature({"Downloads"})
    public void testDuplicateHttpPostDownload_OpenNewTabAndReplace() throws Exception {
        final String url =
                mTestServer.getURL("/chrome/test/data/android/download/get.html");

        // Create the file in advance so that duplicate download infobar can show up.
        File dir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        assertTrue(dir.isDirectory());
        final File file = new File(dir, "test.gzip");
        try {
            if (!file.exists()) {
                assertTrue(file.createNewFile());
            }

            // Open in a new tab again.
            loadUrl(url);
            waitForFocus();

            View currentView = getActivity().getActivityTab().getView();
            TouchCommon.longPressView(currentView);
            getInstrumentation().invokeContextMenuAction(
                    getActivity(), R.id.contextmenu_open_in_new_tab, 0);
            waitForNewTabToStabilize(2);

            goToLastTab();
            assertPollForInfoBarSize(1);

            // Now create two new files by clicking on the infobars.
            assertTrue("OVERWRITE button wasn't found",
                    InfoBarUtil.clickPrimaryButton(getInfoBars().get(0)));
        } finally {
            if (!file.delete()) {
                Log.d(TAG, "Failed to delete test.gzip");
            }
        }
    }

    @MediumTest
    @Feature({"Downloads"})
    public void testUrlEscaping() throws Exception {
        loadUrl(mTestServer.getURL("/chrome/test/data/android/download/urlescaping.html"));
        waitForFocus();
        View currentView = getActivity().getActivityTab().getView();

        EnqueueHttpGetDownloadCallbackHelper callbackHelper = getHttpGetDownloadCallbackHelper();
        int callCount = callbackHelper.getCallCount();
        singleClickView(currentView);
        callbackHelper.waitForCallback(callCount);
        assertEquals(mTestServer.getURL(
                             "/chrome/test/data/android/download/[large]wallpaper.dm"),
                callbackHelper.getDownloadInfo().getUrl());
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
        final InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                updateFailureReason("There should be " + size + " infobar but there are "
                        + getInfoBars().size() + " infobars.");
                return getInfoBars().size() == size && !container.isAnimating();
            }
        });
    }
}
