// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;

import android.app.ActivityManager;
import android.app.ActivityManager.RunningServiceInfo;
import android.content.Context;
import android.os.Build;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeMobileApplication;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.test.util.DisableInTabbedMode;
import org.chromium.content.app.SandboxedProcessService;
import org.chromium.content.browser.test.util.CallbackHelper;

import java.util.List;

/**
 * Tests the how Document mode works on low end devices.
 */
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
@DisableInTabbedMode
public class DocumentModeLowEndTest extends DocumentModeTestBase {

    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    @MediumTest
    public void testNewTabLoadLowEnd() throws Exception {
        launchViaLaunchDocumentInstance(false, HREF_LINK, "href link page");

        final CallbackHelper tabCreatedCallback = new CallbackHelper();
        final CallbackHelper tabLoadStartedCallback = new CallbackHelper();

        final DocumentTabModelSelector selector =
                ChromeMobileApplication.getDocumentTabModelSelector();
        selector.addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onNewTabCreated(final Tab newTab) {
                selector.removeObserver(this);
                tabCreatedCallback.notifyCalled();

                assertFalse(newTab.getWebContents().isLoadingToDifferentDocument());

                newTab.addObserver(new EmptyTabObserver() {
                    @Override
                    public void onPageLoadStarted(Tab tab, String url) {
                        newTab.removeObserver(this);
                        tabLoadStartedCallback.notifyCalled();
                    }
                });
            }
        });

        openLinkInBackgroundTab();

        // Tab should be created, but shouldn't start loading until we switch to it.
        assertEquals(1, tabCreatedCallback.getCallCount());
        assertEquals(0, tabLoadStartedCallback.getCallCount());

        TabModelUtils.setIndex(selector.getCurrentModel(), 1);
        tabLoadStartedCallback.waitForCallback(0);
    }

    /**
     * Tests that "Open in new tab" command doesn't create renderer per tab
     * on low end devices.
     */
    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    @MediumTest
    public void testNewTabRenderersLowEnd() throws Exception {
        launchViaLaunchDocumentInstance(false, HREF_LINK, "href link page");

        // Ignore any side effects that the first background tab might produce.
        openLinkInBackgroundTab();

        int rendererCountBefore = countRenderers();

        final int newTabCount = 5;
        for (int i = 0; i != newTabCount; ++i) {
            openLinkInBackgroundTab();
        }

        int rendererCountAfter = countRenderers();

        assertEquals(rendererCountBefore, rendererCountAfter);
    }

    /**
     * Returns the number of currently running renderer services.
     */
    private int countRenderers() {
        ActivityManager activityManager = (ActivityManager) mContext.getSystemService(
                Context.ACTIVITY_SERVICE);

        int rendererCount = 0;
        List<RunningServiceInfo> serviceInfos = activityManager.getRunningServices(
                Integer.MAX_VALUE);
        for (RunningServiceInfo serviceInfo : serviceInfos) {
            if (serviceInfo.service.getClassName().startsWith(
                    SandboxedProcessService.class.getName())) {
                rendererCount++;
            }
        }

        return rendererCount;
    }
}
