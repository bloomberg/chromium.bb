// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.Context;
import android.os.Build;
import android.support.test.filters.SmallTest;
import android.view.View;

import org.chromium.base.BaseSwitches;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadTestBase;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.widget.Toast;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests that toasts don't trigger HW acceleration.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
public class ToastHWATest extends DownloadTestBase {
    private EmbeddedTestServer mTestServer;

    private static final String URL_PATH = "/chrome/test/data/android/google.html";
    private static final String IMAGE_NAME = "google.png";
    private static final String IMAGE_ID = "logo";
    private static final String LINK_ID = "aboutLink";

    private static final String[] TEST_FILES = {IMAGE_NAME};

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FirstRunStatus.setFirstRunFlowComplete(true);
            }
        });

        deleteFilesInDownloadDirectory(TEST_FILES);
        mTestServer = EmbeddedTestServer.createAndStartServer(getInstrumentation().getContext());
    }

    @Override
    protected void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FirstRunStatus.setFirstRunFlowComplete(false);
            }
        });

        mTestServer.stopAndDestroyServer();
        deleteFilesInDownloadDirectory(TEST_FILES);
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @SmallTest
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    public void testNoRenderThread() throws Exception {
        Utils.assertNoRenderThread();
    }

    /*
     * @MediumTest
     * @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
     * BUG=crbug.com/668217
    */
    @DisabledTest
    public void testDownloadingToast() throws Exception {
        loadUrl(mTestServer.getURL(URL_PATH));
        assertWaitForPageScaleFactorMatch(0.5f);

        int callCount = getChromeDownloadCallCount();

        // Download an image (shows 'Downloading...' toast)
        Tab tab = getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, IMAGE_ID,
                R.id.contextmenu_save_image);

        // Wait for UI activity to settle
        getInstrumentation().waitForIdleSync();

        // Wait for download to finish
        assertTrue(waitForChromeDownloadToFinish(callCount));

        Utils.assertNoRenderThread();
    }

    @SmallTest
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    public void testOpenedInBackgroundToast() throws Exception {
        loadUrl(mTestServer.getURL(URL_PATH));
        assertWaitForPageScaleFactorMatch(0.5f);

        // Open link in a new tab (shows 'Tab Opened In Background' toast)
        Tab tab = getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(this, tab, LINK_ID,
                R.id.contextmenu_open_in_new_tab);

        // Wait for UI activity to settle
        getInstrumentation().waitForIdleSync();

        Utils.assertNoRenderThread();
    }

    @SmallTest
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    public void testToastNoAcceleration() throws Exception {
        // Toasts created on low-end devices shouldn't be HW accelerated.
        assertFalse(isToastAcceleratedWithContext(getActivity()));
        assertFalse(isToastAcceleratedWithContext(getActivity().getApplicationContext()));
    }

    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToastAcceleration() throws Exception {
        // Toasts created on high-end devices should be HW accelerated.
        assertTrue(isToastAcceleratedWithContext(getActivity()));
        assertTrue(isToastAcceleratedWithContext(getActivity().getApplicationContext()));
    }

    private static boolean isToastAcceleratedWithContext(final Context context) throws Exception {
        final AtomicBoolean accelerated = new AtomicBoolean();
        final CallbackHelper listenerCalled = new CallbackHelper();

        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                // We are using Toast.makeText(context, ...) instead of new Toast(context)
                // because that Toast constructor is unused and is deleted by proguard.
                Toast toast = Toast.makeText(context, "Test", Toast.LENGTH_SHORT);
                toast.setView(new View(context) {
                    @Override
                    public void onAttachedToWindow() {
                        super.onAttachedToWindow();
                        accelerated.set(isHardwareAccelerated());
                        listenerCalled.notifyCalled();
                    }
                });
                toast.show();
            }
        });

        listenerCalled.waitForCallback(0);
        return accelerated.get();
    }
}
