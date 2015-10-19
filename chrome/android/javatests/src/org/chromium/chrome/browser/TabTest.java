// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ssl.ConnectionSecurityLevel;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.content.browser.test.util.CallbackHelper;

/**
 * Tests for Tab class.
 */
public class TabTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private Tab mTab;
    private CallbackHelper mOnTitleUpdatedHelper;

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onTitleUpdated(Tab tab) {
            mOnTitleUpdatedHelper.notifyCalled();
        }
    };

    public TabTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTab = getActivity().getActivityTab();
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
        runJavaScriptCodeInCurrentTab("document.title='" + newTitle + "';");
        mOnTitleUpdatedHelper.waitForCallback(currentCallCount);
        assertEquals("title does not update", newTitle, mTab.getTitle());
    }

    /**
     * Verifies a Tab's contents is restored when the Tab is foregrounded
     * after its contents have been destroyed while backgrounded.
     * Note that document mode is explicitly disabled, as the document activity
     * may be fully recreated if its contents is killed while in the background.
     */
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_DOCUMENT_MODE)
    @SmallTest
    @Feature({"Tab"})
    public void testTabRestoredIfKilledWhileActivityStopped() throws Exception {
        // Ensure the tab is showing before stopping the activity.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTab.show(TabSelectionType.FROM_NEW);
            }
        });

        assertFalse(mTab.needsReload());
        assertFalse(mTab.isHidden());
        assertFalse(mTab.isShowingSadTab());

        // Stop the activity and simulate a killed renderer.
        ApplicationTestUtils.fireHomeScreenIntent(getInstrumentation().getTargetContext());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mTab.simulateRendererKilledForTesting(false);
            }
        });

        assertTrue(mTab.needsReload());
        assertTrue(mTab.isHidden());
        assertFalse(mTab.isShowingSadTab());

        ApplicationTestUtils.launchChrome(getInstrumentation().getTargetContext());

        // The tab should be restored and visible.
        assertFalse(mTab.isHidden());
        assertFalse(mTab.needsReload());
        assertFalse(mTab.isShowingSadTab());
    }

    @SmallTest
    @Feature({"Tab"})
    public void testTabSecurityLevel() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertEquals(ConnectionSecurityLevel.NONE, mTab.getSecurityLevel());
            }
        });
    }
}
