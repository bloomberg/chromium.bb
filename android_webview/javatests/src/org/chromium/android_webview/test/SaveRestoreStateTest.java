// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Bundle;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.NavigationEntry;
import org.chromium.content.browser.NavigationHistory;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

public class SaveRestoreStateTest extends AndroidWebViewTestBase {
    private static class TestVars {
        public final TestAwContentsClient contentsClient;
        public final AwTestContainerView testView;
        public final AwContents awContents;
        public final ContentViewCore contentViewCore;

        public TestVars(TestAwContentsClient contentsClient,
                        AwTestContainerView testView) {
            this.contentsClient = contentsClient;
            this.testView = testView;
            this.awContents = testView.getAwContents();
            this.contentViewCore = this.awContents.getContentViewCore();
        }
    }

    private TestVars createNewView() throws Exception {
        TestAwContentsClient contentsClient = new TestAwContentsClient();;
        AwTestContainerView testView = createAwTestContainerViewOnMainSync(contentsClient);
        return new TestVars(contentsClient, testView);
    }

    private TestVars mVars;
    private TestWebServer mWebServer;

    private static final int NUM_NAVIGATIONS = 3;
    private static final String TITLES[] = {
            "page 1 title foo",
            "page 2 title bar",
            "page 3 title baz"
    };
    private static final String PATHS[] = {
            "/p1foo.html",
            "/p2bar.html",
            "/p3baz.html",
    };

    private String mUrls[];

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mVars = createNewView();
        mUrls = new String[NUM_NAVIGATIONS];
        mWebServer = new TestWebServer(false);
    }

    @Override
    public void tearDown() throws Exception {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
        super.tearDown();
    }

    private void setServerResponseAndLoad(TestVars vars, int upto) throws Throwable {
        for (int i = 0; i < upto; ++i) {
            String html = CommonResources.makeHtmlPageFrom(
                    "<title>" + TITLES[i] + "</title>",
                    "");
            mUrls[i] = mWebServer.setResponse(PATHS[i], html, null);

            loadUrlSync(vars.awContents,
                        vars.contentsClient.getOnPageFinishedHelper(),
                        mUrls[i]);
        }
    }

    private NavigationHistory getNavigationHistoryOnUiThread(
            final TestVars vars) throws Throwable{
        return runTestOnUiThreadAndGetResult(new Callable<NavigationHistory>() {
            @Override
            public NavigationHistory call() throws Exception {
                return vars.contentViewCore.getNavigationHistory();
            }
        });
    }

    private void checkHistoryItemList(TestVars vars) throws Throwable {
        NavigationHistory history = getNavigationHistoryOnUiThread(vars);
        assertEquals(NUM_NAVIGATIONS, history.getEntryCount());
        assertEquals(NUM_NAVIGATIONS - 1, history.getCurrentEntryIndex());

        // Note this is not meant to be a thorough test of NavigationHistory,
        // but is only meant to test enough to make sure state is restored.
        // See NavigationHistoryTest for more thorough tests.
        for (int i = 0; i < NUM_NAVIGATIONS; ++i) {
            assertEquals(mUrls[i], history.getEntryAtIndex(i).getOriginalUrl());
            assertEquals(mUrls[i], history.getEntryAtIndex(i).getUrl());
            assertEquals(TITLES[i], history.getEntryAtIndex(i).getTitle());
        }
    }

    private TestVars saveAndRestoreStateOnUiThread(final TestVars vars) throws Throwable {
        final TestVars restoredVars = createNewView();
        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                Bundle bundle = new Bundle();
                boolean result = vars.awContents.saveState(bundle);
                assertTrue(result);
                result = restoredVars.awContents.restoreState(bundle);
                assertTrue(result);
            }
        });
        return restoredVars;
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSaveRestoreStateWithTitle() throws Throwable {
        setServerResponseAndLoad(mVars, 1);
        final TestVars restoredVars = saveAndRestoreStateOnUiThread(mVars);
        assertTrue(pollOnUiThread(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return TITLES[0].equals(restoredVars.contentViewCore.getTitle()) &&
                       TITLES[0].equals(restoredVars.contentsClient.getUpdatedTitle());
            }
        }));
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSaveRestoreStateWithHistoryItemList() throws Throwable {
        setServerResponseAndLoad(mVars, NUM_NAVIGATIONS);
        TestVars restoredVars = saveAndRestoreStateOnUiThread(mVars);
        checkHistoryItemList(restoredVars);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRestoreFromInvalidStateFails() throws Throwable {
        final Bundle invalidState = new Bundle();
        invalidState.putByteArray(AwContents.SAVE_RESTORE_STATE_KEY,
                                  "invalid state".getBytes());
        boolean result = runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mVars.awContents.restoreState(invalidState);
            }
        });
        assertFalse(result);
    }

    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSaveStateForNoNavigationFails() throws Throwable {
        final Bundle state = new Bundle();
        boolean result = runTestOnUiThreadAndGetResult(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return mVars.awContents.restoreState(state);
            }
        });
        assertFalse(result);
    }
}
