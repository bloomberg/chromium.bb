// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.test.suitebuilder.annotation.SmallTest;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewCore;

public class UserAgentTest extends AndroidWebViewTestBase {

    private TestAwContentsClient mContentsClient;
    private ContentViewCore mContentViewCore;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContentsClient = new TestAwContentsClient();
        mContentViewCore =
                createAwTestContainerViewOnMainSync(mContentsClient).getContentViewCore();
    }

    /**
     * Test for b/6404375. Verify that the UA string doesn't contain
     * two spaces before the Android build name.
     */
    @SmallTest
    @Feature({"Android-WebView"})
    public void testNoExtraSpaceBeforeBuildName() throws Throwable {
        getContentSettingsOnUiThread(mContentViewCore).setJavaScriptEnabled(true);
        loadDataSync(
            mContentViewCore,
            mContentsClient.getOnPageFinishedHelper(),
            // Spaces are replaced with underscores to avoid consecutive spaces compression.
            "<html>" +
            "<body onload='document.title=navigator.userAgent.replace(/ /g, \"_\")'></body>" +
            "</html>",
            "text/html", false);
        final String ua = getTitleOnUiThread(mContentViewCore);
        Matcher matcher = Pattern.compile("Android_[^;]+;_[^_]").matcher(ua);
        assertTrue(matcher.find());
    }
}
