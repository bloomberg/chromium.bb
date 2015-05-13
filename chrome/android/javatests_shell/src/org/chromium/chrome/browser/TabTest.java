// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ssl.ConnectionSecurityHelperSecurityLevel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.content.browser.test.util.CallbackHelper;

/**
 * Tests for Tab class.
 */
public class TabTest extends ChromeShellTestBase {
    private Tab mTab;
    private CallbackHelper mOnTitleUpdatedHelper;

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onTitleUpdated(Tab tab) {
            mOnTitleUpdatedHelper.notifyCalled();
        }
    };

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        launchChromeShellWithBlankPage();
        waitForActiveShellToBeDoneLoading();
        mTab = getActivity().getActiveTab();
        mTab.addObserver(mTabObserver);
        mOnTitleUpdatedHelper = new CallbackHelper();
    }

    @SmallTest
    @Feature({"Tab"})
    public void testTitleDelayUpdate() throws Throwable {
        final String oldTitle = "oldTitle";
        final String newTitle = "newTitle";

        loadUrl("data:text/html;charset=utf-8,<html><head><title>"
                + oldTitle + "</title></head><body/></html>");
        assertEquals("title does not match initial title", oldTitle, mTab.getTitle());
        int currentCallCount = mOnTitleUpdatedHelper.getCallCount();
        loadUrl("javascript:document.title='" + newTitle + "';");
        mOnTitleUpdatedHelper.waitForCallback(currentCallCount);
        assertEquals("title does not update", newTitle, mTab.getTitle());
    }

    @SmallTest
    @Feature({"Tab"})
    public void testTabRestoredIfKilledWhileActivityStopped() {
        launchChromeShellWithBlankPage();
        final Tab tab = getActivity().getActiveTab();

        // Ensure the tab is showing before stopping the activity.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tab.show(TabSelectionType.FROM_NEW);
            }
        });

        assertFalse(tab.needsReload());
        assertFalse(tab.isHidden());
        assertFalse(tab.isShowingSadTab());

        // Stop the activity and simulate a killed renderer.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getInstrumentation().callActivityOnPause(getActivity());
                getInstrumentation().callActivityOnStop(getActivity());
                tab.simulateRendererKilledForTesting(false);
            }
        });

        assertTrue(tab.needsReload());
        assertFalse(tab.isHidden());
        assertFalse(tab.isShowingSadTab());

        // Resume the activity.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getInstrumentation().callActivityOnStart(getActivity());
                getInstrumentation().callActivityOnResume(getActivity());
            }
        });

        // The tab should be restored and visible.
        assertFalse(tab.isHidden());
        assertFalse(tab.needsReload());
        assertFalse(tab.isShowingSadTab());
    }

    @SmallTest
    @Feature({"Tab"})
    public void testTabSecurityLevel() {
        assertEquals(ConnectionSecurityHelperSecurityLevel.NONE, mTab.getSecurityLevel());
    }
}
