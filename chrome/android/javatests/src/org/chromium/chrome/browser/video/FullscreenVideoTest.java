// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video;

import android.os.Environment;
import android.test.FlakyTest;
import android.view.KeyEvent;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.TestTouchUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Test suite for fullscreen video implementation.
 */
public class FullscreenVideoTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final int TEST_TIMEOUT = 3000;
    private boolean mIsTabFullscreen = false;

    private class FullscreenTabObserver extends EmptyTabObserver {
        @Override
        public void onToggleFullscreenMode(Tab tab, boolean enable) {
            mIsTabFullscreen = enable;
        }
    }

    public FullscreenVideoTest() {
        super(ChromeActivity.class);
        mSkipCheckHttpServer = true;
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    /**
     * Test that when playing a fullscreen video, hitting the back button will let the tab
     * exit fullscreen mode without changing its URL.
     *
     * @MediumTest
     * crbug.com/458368.
     */
    @FlakyTest
    public void testExitFullscreenNotifiesTabObservers() throws InterruptedException {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
        try {
            String url = testServer.getURL(
                    "/chrome/test/data/android/media/video-fullscreen.html");
            loadUrl(url);
            Tab tab = getActivity().getActivityTab();
            FullscreenTabObserver observer = new FullscreenTabObserver();
            tab.addObserver(observer);

            TestTouchUtils.singleClickView(getInstrumentation(), tab.getView(), 500, 500);
            waitForVideoToEnterFullscreen();
            // Key events have to be dispached on UI thread.
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    getActivity().dispatchKeyEvent(
                            new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK));
                    getActivity().dispatchKeyEvent(
                            new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK));
                }
            });

            waitForTabToExitFullscreen();
            assertEquals("URL mismatch after exiting fullscreen video",
                    url, getActivity().getActivityTab().getUrl());
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    void waitForVideoToEnterFullscreen() throws InterruptedException {
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mIsTabFullscreen;
            }
        }, TEST_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    void waitForTabToExitFullscreen() throws InterruptedException {
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !mIsTabFullscreen;
            }
        }, TEST_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
