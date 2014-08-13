// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.NavigationTransitionDelegate;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Test suite for navigation transition listeners.
 */
public class TransitionTest extends ContentShellTestBase {

    private static final String URL_1 = UrlUtils.encodeHtmlDataUri("<html>1</html>");
    private static final String URL_2 = "/2.html";
    private static final String URL_2_DATA = "<html>2</html>";
    private static final String URL_3 = "/3.html";
    private static final String URL_3_DATA = "<html>3</html>";

    static class TestNavigationTransitionDelegate implements NavigationTransitionDelegate {
        private boolean mDidCallDefer = false;
        private boolean mDidCallWillHandleDefer = false;
        private boolean mDidCallAddStylesheet = false;
        private boolean mHandleDefer = false;
        private ArrayList<String> mTransitionStylesheets;
        private ContentViewCore mContentViewCore;
        private String mTransitionEnteringColor;

        TestNavigationTransitionDelegate(ContentViewCore contentViewCore, boolean handleDefer) {
            mContentViewCore = contentViewCore;
            mHandleDefer = handleDefer;
            mTransitionStylesheets = new ArrayList<String>();
        }

        @Override
        public void didDeferAfterResponseStarted(String markup, String cssSelector,
                String enteringColor) {
            mDidCallDefer = true;
            mContentViewCore.resumeResponseDeferredAtStart();
            mTransitionEnteringColor = enteringColor;
        }

        @Override
        public boolean willHandleDeferAfterResponseStarted() {
            return mHandleDefer;
        }

        @Override
        public void addEnteringStylesheetToTransition(String stylesheet) {
            mDidCallAddStylesheet = true;
            mTransitionStylesheets.add(stylesheet);
        }

        @Override
        public void didStartNavigationTransitionForFrame(long frameId) {
        }

        public boolean getDidCallDefer() {
            return mDidCallDefer;
        }

        public boolean getDidCallWillHandlerDefer() {
            return mDidCallWillHandleDefer;
        }

        public boolean getDidCallAddStylesheet() {
            return mDidCallAddStylesheet;
        }

        public ArrayList<String> getTransitionStylesheets() {
            return mTransitionStylesheets;
        }

        public String getTransitionEnteringColor() {
            return mTransitionEnteringColor;
        }
    };

    private static List<Pair<String, String>> createHeadersList(
        String[] namesAndValues) {
        List<Pair<String, String>> result =
            new ArrayList<Pair<String, String>>();
        for (int i = 0; i < namesAndValues.length; i += 2)
            result.add(Pair.create(namesAndValues[i], namesAndValues[i + 1]));
        return result;
    }

    /**
     * Tests that the listener recieves DidDeferAfterResponseStarted if we specify that
     * the transition is handled.
     */
    @SmallTest
    public void testDidDeferAfterResponseStartedCalled() throws Throwable {
        ContentShellActivity activity = launchContentShellWithUrl(URL_1);
        waitForActiveShellToBeDoneLoading();
        ContentViewCore contentViewCore = activity.getActiveContentViewCore();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(contentViewCore);

        contentViewCore.getWebContents().setHasPendingNavigationTransitionForTesting();
        TestNavigationTransitionDelegate delegate = new TestNavigationTransitionDelegate(
                contentViewCore,
                true);
        contentViewCore.getWebContents().setNavigationTransitionDelegate(delegate);

        loadUrl(contentViewCore, testCallbackHelperContainer, new LoadUrlParams(URL_1));

        assertTrue("didDeferAfterResponseStarted called.", delegate.getDidCallDefer());
    }

    /**
     * Tests that the listener does not receive DidDeferAfterResponseStarted if we specify that
     * the transition is handled.
     */
    @SmallTest
    public void testDidDeferAfterResponseStartedNotCalled() throws Throwable {
        ContentShellActivity activity = launchContentShellWithUrl(URL_1);
        waitForActiveShellToBeDoneLoading();
        ContentViewCore contentViewCore = activity.getActiveContentViewCore();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(contentViewCore);

        contentViewCore.getWebContents().setHasPendingNavigationTransitionForTesting();
        TestNavigationTransitionDelegate delegate = new TestNavigationTransitionDelegate(
                contentViewCore,
                false);
        contentViewCore.getWebContents().setNavigationTransitionDelegate(delegate);

        loadUrl(contentViewCore, testCallbackHelperContainer, new LoadUrlParams(URL_1));

        assertFalse("didDeferAfterResponseStarted called.", delegate.getDidCallDefer());
    }

    /**
     * Tests that the resource handler doesn't query the listener if no transition is pending.
     */
    @SmallTest
    public void testWillHandleDeferAfterResponseStartedNotCalled() throws Throwable {
        ContentShellActivity activity = launchContentShellWithUrl(URL_1);
        waitForActiveShellToBeDoneLoading();
        ContentViewCore contentViewCore = activity.getActiveContentViewCore();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(contentViewCore);

        TestNavigationTransitionDelegate delegate = new TestNavigationTransitionDelegate(
                contentViewCore,
                false);
        contentViewCore.getWebContents().setNavigationTransitionDelegate(delegate);

        loadUrl(contentViewCore, testCallbackHelperContainer, new LoadUrlParams(URL_1));

        assertFalse("didDeferAfterResponseStarted called.", delegate.getDidCallDefer());
        assertFalse("willHandleDeferAfterResponseStarted called.",
                delegate.getDidCallWillHandlerDefer());
    }

    /**
     * Tests that the listener receives addStylesheetToTransition if we specify
     * that there are entering transition stylesheet.
     */
    @SmallTest
    public void testAddStylesheetToTransitionCalled() throws Throwable {
        TestWebServer webServer = null;
        try {
          webServer = new TestWebServer(false);

          final String url2 = webServer.setResponse(URL_2, URL_2_DATA, null);
          ContentShellActivity activity = launchContentShellWithUrl(url2);
          waitForActiveShellToBeDoneLoading();
          ContentViewCore contentViewCore = activity.getActiveContentViewCore();
          TestCallbackHelperContainer testCallbackHelperContainer =
              new TestCallbackHelperContainer(contentViewCore);
          contentViewCore.getWebContents().setHasPendingNavigationTransitionForTesting();
          TestNavigationTransitionDelegate delegate =
              new TestNavigationTransitionDelegate(contentViewCore, true);
          contentViewCore.getWebContents().setNavigationTransitionDelegate(delegate);

          int currentCallCount = testCallbackHelperContainer
              .getOnPageFinishedHelper().getCallCount();
          String[] headers = {
              "link",
              "<transition0.css>;rel=transition-entering-stylesheet;scope=*",
              "link",
              "<transition1.css>;rel=transition-entering-stylesheet;scope=*",
              "link",
              "<transition2.css>;rel=transition-entering-stylesheet;scope=*"
          };
          final String url3 = webServer.setResponse(URL_3,
              URL_3_DATA,
              createHeadersList(headers));
          LoadUrlParams url3_params = new LoadUrlParams(url3);
          loadUrl(contentViewCore, testCallbackHelperContainer, url3_params);
          testCallbackHelperContainer.getOnPageFinishedHelper().waitForCallback(
              currentCallCount,
              1,
              10000,
              TimeUnit.MILLISECONDS);

          assertTrue("addStylesheetToTransition called.",
              delegate.getDidCallAddStylesheet());
          assertTrue("Three stylesheets are added",
              delegate.getTransitionStylesheets().size() == 3);
        } finally {
          if (webServer != null)
            webServer.shutdown();
        }
    }

    /**
     * Tests that the listener receives addStylesheetToTransition if we specify
     * that there are no entering transition stylesheet.
     */
    @SmallTest
    public void testAddStylesheetToTransitionNotCalled() throws Throwable {
        TestWebServer webServer = null;
        try {
          webServer = new TestWebServer(false);

          final String url2 = webServer.setResponse(URL_2, URL_2_DATA, null);
          ContentShellActivity activity = launchContentShellWithUrl(url2);
          waitForActiveShellToBeDoneLoading();
          ContentViewCore contentViewCore = activity.getActiveContentViewCore();
          TestCallbackHelperContainer testCallbackHelperContainer =
              new TestCallbackHelperContainer(contentViewCore);
          contentViewCore.getWebContents().setHasPendingNavigationTransitionForTesting();
          TestNavigationTransitionDelegate delegate =
              new TestNavigationTransitionDelegate(contentViewCore, true);
          contentViewCore.getWebContents().setNavigationTransitionDelegate(delegate);

          int currentCallCount = testCallbackHelperContainer
              .getOnPageFinishedHelper().getCallCount();
          final String url3 = webServer.setResponse(URL_3, URL_3_DATA, null);
          LoadUrlParams url3_params = new LoadUrlParams(url3);
          loadUrl(contentViewCore, testCallbackHelperContainer, url3_params);
          testCallbackHelperContainer.getOnPageFinishedHelper().waitForCallback(
              currentCallCount,
              1,
              10000,
              TimeUnit.MILLISECONDS);

          assertFalse("addStylesheetToTransition is not called.",
              delegate.getDidCallAddStylesheet());
          assertTrue("No stylesheets are added",
              delegate.getTransitionStylesheets().size() == 0);
        } finally {
          if (webServer != null)
            webServer.shutdown();
        }
    }

    /**
     * Tests that the listener receives the entering color if it's specified in the
     * response headers.
     */
    @SmallTest
    public void testParseTransitionEnteringColor() throws Throwable {
        TestWebServer webServer = null;
        try {
            webServer = new TestWebServer(false);

            final String url2 = webServer.setResponse(URL_2, URL_2_DATA, null);
            ContentShellActivity activity = launchContentShellWithUrl(url2);
            waitForActiveShellToBeDoneLoading();
            ContentViewCore contentViewCore = activity.getActiveContentViewCore();
            TestCallbackHelperContainer testCallbackHelperContainer =
                    new TestCallbackHelperContainer(contentViewCore);
            contentViewCore.getWebContents().setHasPendingNavigationTransitionForTesting();
            TestNavigationTransitionDelegate delegate =
                    new TestNavigationTransitionDelegate(contentViewCore, true);
            contentViewCore.getWebContents().setNavigationTransitionDelegate(delegate);

            String transitionEnteringColor = "#00FF00";

            int currentCallCount = testCallbackHelperContainer
                    .getOnPageFinishedHelper().getCallCount();
            String[] headers = {
                    "X-Transition-Entering-Color",
                    transitionEnteringColor,
            };
            final String url3 = webServer.setResponse(URL_3,
                    URL_3_DATA,
                    createHeadersList(headers));
            LoadUrlParams url3Params = new LoadUrlParams(url3);
            loadUrl(contentViewCore, testCallbackHelperContainer, url3Params);
            testCallbackHelperContainer.getOnPageFinishedHelper().waitForCallback(
                    currentCallCount,
                    1,
                    10000,
                    TimeUnit.MILLISECONDS);

            assertTrue("X-Transition-Entering-Color parsed correctly.",
                    TextUtils.equals(
                            delegate.getTransitionEnteringColor(),
                            transitionEnteringColor));
        } finally {
            if (webServer != null) webServer.shutdown();
        }
    }
}
