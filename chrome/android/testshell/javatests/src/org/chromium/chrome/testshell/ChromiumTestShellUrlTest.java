// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.ContentViewRenderView;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

public class ChromiumTestShellUrlTest extends ChromiumTestShellTestBase {
    // URL used for base tests.
    private static final String URL = "data:text";

    @SmallTest
    @Feature({"Main"})
    public void testBaseStartup() throws InterruptedException {
        ChromiumTestShellActivity activity = launchChromiumTestShellWithUrl(URL);
        waitForActiveShellToBeDoneLoading();

        // Make sure the activity was created as expected.
        assertNotNull(activity);
    }

    @SmallTest
    @Feature({"Main"})
    public void testChromeWelcomePageLoads() throws InterruptedException {
        String welcomeUrl = "chrome://welcome/";
        final ChromiumTestShellActivity activity = launchChromiumTestShellWithUrl(welcomeUrl);
        waitForActiveShellToBeDoneLoading();

        // Make sure the activity was created as expected.
        assertNotNull(activity);

        // Ensure we have a ContentView and ContentViewCore.
        final AtomicReference<ContentView> contentView = new AtomicReference<ContentView>();
        final AtomicReference<ContentViewCore> contentViewCore =
                new AtomicReference<ContentViewCore>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ContentView activeContentView = activity.getActiveContentView();
                contentView.set(activeContentView);
                if (activeContentView != null) {
                    contentViewCore.set(activeContentView.getContentViewCore());
                }
            }
        });
        assertNotNull(contentView.get());
        assertNotNull(contentViewCore.get());

        // Ensure the correct page has been loaded, ie. not interstitial, and title/url should
        // be sane. Note, a typical correct title is: "Welcome to Chromium", whereas a wrong one
        // would be on the form "chrome://welcome/ is not available".
        final AtomicBoolean isShowingInterstitialPage = new AtomicBoolean();
        final AtomicReference<String> url = new AtomicReference<String>();
        final AtomicReference<String> title = new AtomicReference<String>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                isShowingInterstitialPage.set(contentViewCore.get().isShowingInterstitialPage());
                url.set(contentViewCore.get().getUrl());
                title.set(contentViewCore.get().getTitle());
            }
        });
        assertFalse("Showed interstitial page instead of welcome page",
                isShowingInterstitialPage.get());
        assertNotNull("URL was null", url.get());
        assertTrue("URL did not contain: " + welcomeUrl + ". Was: " + url.get(),
                url.get().contains(welcomeUrl));
        assertNotNull("Title was null", title.get());
        assertFalse("Title should not contain: " + welcomeUrl + ". Was: " + title.get(),
                title.get().toLowerCase().contains(welcomeUrl));
    }

    /**
     * Tests that creating an extra ContentViewRenderView does not cause an assert because we would
     * initialize the compositor twice http://crbug.com/162312
     */
    @SmallTest
    @Feature({"Main"})
    public void testCompositorInit() throws InterruptedException {
        // Start the ChromiumTestShell, this loads the native library and create an instance of
        // ContentViewRenderView.
        final ChromiumTestShellActivity activity = launchChromiumTestShellWithUrl(URL);
        waitForActiveShellToBeDoneLoading();

        // Now create a new ContentViewRenderView, it should not assert.
        try {
            runTestOnUiThread(new Runnable() {
                @Override
                public void run() {
                    ContentViewRenderView contentViewRenderView =
                            new ContentViewRenderView(getInstrumentation().getTargetContext());
                    contentViewRenderView.setCurrentContentView(activity.getActiveContentView());
                }
            });
        } catch (Throwable e) {
            fail("Could not create a ContentViewRenderView: " + e);
        }
    }
}