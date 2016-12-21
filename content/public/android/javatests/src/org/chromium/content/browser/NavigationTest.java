// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.filters.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellTestBase;

/**
 * Tests for various aspects of navigation.
 */
public class NavigationTest extends ContentShellTestBase {

    private static final String URL_1 = UrlUtils.encodeHtmlDataUri("<html>1</html>");
    private static final String URL_2 = UrlUtils.encodeHtmlDataUri("<html>2</html>");
    private static final String URL_3 = UrlUtils.encodeHtmlDataUri("<html>3</html>");
    private static final String URL_4 = UrlUtils.encodeHtmlDataUri("<html>4</html>");
    private static final String URL_5 = UrlUtils.encodeHtmlDataUri("<html>5</html>");
    private static final String URL_6 = UrlUtils.encodeHtmlDataUri("<html>6</html>");
    private static final String URL_7 = UrlUtils.encodeHtmlDataUri("<html>7</html>");

    private void goBack(final NavigationController navigationController,
            TestCallbackHelperContainer testCallbackHelperContainer) throws Throwable {
        handleBlockingCallbackAction(
                testCallbackHelperContainer.getOnPageFinishedHelper(),
                new Runnable() {
                    @Override
                    public void run() {
                        navigationController.goBack();
                    }
                });
    }

    private void reload(final NavigationController navigationController,
            TestCallbackHelperContainer testCallbackHelperContainer) throws Throwable {
        handleBlockingCallbackAction(
                testCallbackHelperContainer.getOnPageFinishedHelper(),
                new Runnable() {
                    @Override
                    public void run() {
                        navigationController.reload(true);
                    }
                });
    }

    @MediumTest
    @Feature({"Navigation"})
    @FlakyTest
    public void testDirectedNavigationHistory() throws Throwable {
        ContentShellActivity activity = launchContentShellWithUrl(URL_1);
        waitForActiveShellToBeDoneLoading();
        ContentViewCore contentViewCore = activity.getActiveContentViewCore();
        NavigationController navigationController = contentViewCore.getWebContents()
                .getNavigationController();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(contentViewCore);

        loadUrl(navigationController, testCallbackHelperContainer, new LoadUrlParams(URL_2));
        loadUrl(navigationController, testCallbackHelperContainer, new LoadUrlParams(URL_3));
        loadUrl(navigationController, testCallbackHelperContainer, new LoadUrlParams(URL_4));
        loadUrl(navigationController, testCallbackHelperContainer, new LoadUrlParams(URL_5));
        loadUrl(navigationController, testCallbackHelperContainer, new LoadUrlParams(URL_6));
        loadUrl(navigationController, testCallbackHelperContainer, new LoadUrlParams(URL_7));

        NavigationHistory history = navigationController.getDirectedNavigationHistory(false, 3);
        assertEquals(3, history.getEntryCount());
        assertEquals(URL_6, history.getEntryAtIndex(0).getUrl());
        assertEquals(URL_5, history.getEntryAtIndex(1).getUrl());
        assertEquals(URL_4, history.getEntryAtIndex(2).getUrl());

        history = navigationController.getDirectedNavigationHistory(true, 3);
        assertEquals(history.getEntryCount(), 0);

        goBack(navigationController, testCallbackHelperContainer);
        goBack(navigationController, testCallbackHelperContainer);
        goBack(navigationController, testCallbackHelperContainer);

        history = navigationController.getDirectedNavigationHistory(false, 4);
        assertEquals(3, history.getEntryCount());
        assertEquals(URL_3, history.getEntryAtIndex(0).getUrl());
        assertEquals(URL_2, history.getEntryAtIndex(1).getUrl());
        assertEquals(URL_1, history.getEntryAtIndex(2).getUrl());

        history = navigationController.getDirectedNavigationHistory(true, 4);
        assertEquals(3, history.getEntryCount());
        assertEquals(URL_5, history.getEntryAtIndex(0).getUrl());
        assertEquals(URL_6, history.getEntryAtIndex(1).getUrl());
        assertEquals(URL_7, history.getEntryAtIndex(2).getUrl());
    }

    /**
     * Tests whether a page was successfully reloaded.
     * Checks to make sure that OnPageFinished events were fired and that the timestamps of when
     * the page loaded are different after the reload.
     */
    @MediumTest
    @Feature({"Navigation"})
    public void testPageReload() throws Throwable {
        final String htmlLoadTime = "<html><head>"
                + "<script type=\"text/javascript\">var loadTimestamp = new Date().getTime();"
                + "function getLoadtime() { return loadTimestamp; }</script></head></html>";
        final String urlLoadTime = UrlUtils.encodeHtmlDataUri(htmlLoadTime);

        ContentShellActivity activity = launchContentShellWithUrl(urlLoadTime);
        waitForActiveShellToBeDoneLoading();
        ContentViewCore contentViewCore = activity.getActiveContentViewCore();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(contentViewCore);
        OnEvaluateJavaScriptResultHelper javascriptHelper = new OnEvaluateJavaScriptResultHelper();

        // Grab the first timestamp.
        javascriptHelper.evaluateJavaScriptForTests(
                contentViewCore.getWebContents(), "getLoadtime();");
        javascriptHelper.waitUntilHasValue();
        String firstTimestamp = javascriptHelper.getJsonResultAndClear();
        assertNotNull("Timestamp was null.", firstTimestamp);

        // Grab the timestamp after a reload and make sure they don't match.
        reload(contentViewCore.getWebContents().getNavigationController(),
                testCallbackHelperContainer);
        javascriptHelper.evaluateJavaScriptForTests(
                contentViewCore.getWebContents(), "getLoadtime();");
        javascriptHelper.waitUntilHasValue();
        String secondTimestamp = javascriptHelper.getJsonResultAndClear();
        assertNotNull("Timestamp was null.", secondTimestamp);
        assertFalse("Timestamps matched.", firstTimestamp.equals(secondTimestamp));
    }
}
