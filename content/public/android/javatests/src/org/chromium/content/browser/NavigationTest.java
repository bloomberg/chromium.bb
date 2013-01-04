// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_shell.ContentShellActivity;
import org.chromium.content_shell.ContentShellTestBase;

/**
 * Tests for various aspects of navigation.
 */
public class NavigationTest extends ContentShellTestBase {

    private static final String URL_1 = "data:text/html;utf-8,<html>1</html>";
    private static final String URL_2 = "data:text/html;utf-8,<html>2</html>";
    private static final String URL_3 = "data:text/html;utf-8,<html>3</html>";
    private static final String URL_4 = "data:text/html;utf-8,<html>4</html>";
    private static final String URL_5 = "data:text/html;utf-8,<html>5</html>";
    private static final String URL_6 = "data:text/html;utf-8,<html>6</html>";
    private static final String URL_7 = "data:text/html;utf-8,<html>7</html>";

    private void goBack(final ContentView contentView,
            TestCallbackHelperContainer testCallbackHelperContainer) throws Throwable {
        handleBlockingCallbackAction(
                testCallbackHelperContainer.getOnPageFinishedHelper(),
                new Runnable() {
                    @Override
                    public void run() {
                        contentView.goBack();
                    }
                });
    }

    @MediumTest
    @Feature({"Navigation"})
    public void testDirectedNavigationHistory() throws Throwable {
        ContentShellActivity activity = launchContentShellWithUrl(URL_1);
        waitForActiveShellToBeDoneLoading();
        ContentView contentView = activity.getActiveContentView();
        TestCallbackHelperContainer testCallbackHelperContainer =
                new TestCallbackHelperContainer(contentView);

        loadUrl(contentView, testCallbackHelperContainer, new LoadUrlParams(URL_2));
        loadUrl(contentView, testCallbackHelperContainer, new LoadUrlParams(URL_3));
        loadUrl(contentView, testCallbackHelperContainer, new LoadUrlParams(URL_4));
        loadUrl(contentView, testCallbackHelperContainer, new LoadUrlParams(URL_5));
        loadUrl(contentView, testCallbackHelperContainer, new LoadUrlParams(URL_6));
        loadUrl(contentView, testCallbackHelperContainer, new LoadUrlParams(URL_7));

        ContentViewCore contentViewCore = contentView.getContentViewCore();
        NavigationHistory history = contentViewCore
                .getDirectedNavigationHistory(false, 3);
        assertEquals(3, history.getEntryCount());
        assertEquals(URL_6, history.getEntryAtIndex(0).getUrl());
        assertEquals(URL_5, history.getEntryAtIndex(1).getUrl());
        assertEquals(URL_4, history.getEntryAtIndex(2).getUrl());

        history = contentView.getContentViewCore()
                .getDirectedNavigationHistory(true, 3);
        assertEquals(history.getEntryCount(), 0);

        goBack(contentView, testCallbackHelperContainer);
        goBack(contentView, testCallbackHelperContainer);
        goBack(contentView, testCallbackHelperContainer);

        history = contentViewCore.getDirectedNavigationHistory(false, 4);
        assertEquals(3, history.getEntryCount());
        assertEquals(URL_3, history.getEntryAtIndex(0).getUrl());
        assertEquals(URL_2, history.getEntryAtIndex(1).getUrl());
        assertEquals(URL_1, history.getEntryAtIndex(2).getUrl());

        history = contentViewCore.getDirectedNavigationHistory(true, 4);
        assertEquals(3, history.getEntryCount());
        assertEquals(URL_5, history.getEntryAtIndex(0).getUrl());
        assertEquals(URL_6, history.getEntryAtIndex(1).getUrl());
        assertEquals(URL_7, history.getEntryAtIndex(2).getUrl());
    }

}
